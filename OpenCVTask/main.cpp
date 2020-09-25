#include <iostream>
#include <fstream>
#include <future>
#include <codecvt>
#include <unordered_map>

#include "NvDecoder/NvDecoder.h"

#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <moonvdec.h>
#include <comet.h>
#include <nebulanetwork.h>
#include <engine.h>

#define checkAPI(expression) { int apiResult = expression; assert(apiResult >= 0); }

void CALLBACK_CONV logCB(const char* msg) {
	std::cout << msg << std::endl;
}

std::string defContent;
const char* CALLBACK_CONV defLoadCB(const char* name) {
	std::ifstream file(name);
	std::stringstream buf;
	buf << file.rdbuf();
	defContent = buf.str();
	return defContent.data();
}

std::string resContent;
void CALLBACK_CONV resLoadCB(const char* name, void** data, uint64_t* size) {
	std::ifstream file(name, std::ios::binary | std::ios::in);
	resContent = std::string(std::istreambuf_iterator<char>(file.rdbuf()), {});
	*data = (void*)resContent.data();
	*size = resContent.size();
}

std::promise<void> startPromise, stopPromise;
void CALLBACK_CONV actionCB(SatLib_Component_Action action, void* aData) {
	if(action == SATLIB_COMPONENT_START)
		startPromise.set_value();
	else if(action == SATLIB_COMPONENT_STOP)
		stopPromise.set_value();
}

CUdevice decodeDevice = 0;
CUcontext decodeCtx = nullptr;
NvDecoder* decoder = nullptr;
std::mutex decoderMutex;
void InitializeDecoder() {
	cuInit(0);

	CUdevice decodeDevice = 0;
	cuDeviceGet(&decodeDevice, 0);
	char szDeviceName[80];
	cuDeviceGetName(szDeviceName, sizeof(szDeviceName), decodeDevice);
	std::cout << "GPU in use: " << szDeviceName << std::endl;
	if(cuCtxCreate(&decodeCtx, 0, decodeDevice) != CUDA_SUCCESS) {
		logCB("Could not create CUDA context");
		return;
	}
	decoder = new NvDecoder(decodeCtx, 0, 0, false, cudaVideoCodec_H264, NULL, true);
}

// Calling multiple times is safe
void DestroyDecoder() {
	delete decoder;
	decoder = nullptr;

	if(decodeCtx != nullptr) {
		if(cuCtxDestroy(decodeCtx) != CUDA_SUCCESS) {
			logCB("Could not destroy CUDA context");
		}
		decodeCtx = nullptr;
	}
}

sbcTopicID tptid, itid;

struct {
	std::atomic<double> MinLineLength = 100.0, MaxLineGap = 10.0;
	std::atomic<int> CannyLow = 100, CannyHigh = 200, ThresholdHough = 5;
} ocvParams;
sbcCometID outLinesComet, frameSlotReadyComet;
sbcEltID linesElt, lineCountElt, srfWidthElt, srfHeightElt;

std::mutex frameMutex;
std::condition_variable frameCV;

std::vector<const uint8_t*> inFrames;
std::vector<unsigned long> inFrameSizes;
std::mutex frameQueueMutex;
std::atomic<int> globalFrameNumber = 0;
std::atomic<bool> processingFrames = true;
std::atomic<unsigned> concurrentCount = 0;
unsigned concurrentMax = 0;

void SignalFrameSlotReady() {
	SatLib_Nebula_SendComet("frameSlotReady", frameSlotReadyComet);
}

void processFrame(const uint8_t* frame, unsigned long frameSize) {
	uint8_t** outFrames;
	uint8_t* myFrame;
	int outFrameCount = 0;
	int width = 0, height = 0;
	auto start = std::chrono::system_clock::now();
	{
		std::lock_guard<std::mutex> lk(decoderMutex);	// DecodeLockFrame, GetWidth/Height are not thread safe, gate them
		if(!decoder->DecodeLockFrame(frame, frameSize, &outFrames, &outFrameCount, CUVID_PKT_ENDOFPICTURE, nullptr, globalFrameNumber.fetch_add(1, std::memory_order_relaxed))) {
			logCB("Error on decode");
			return;
		}
		myFrame = outFrames[0];
		width = decoder->GetWidth();
		height = decoder->GetHeight();
	}
	//std::cout << "Decode time: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() << std::endl;

	if(outFrameCount != 1) {
		logCB(("Error received frame count: " + std::to_string(outFrameCount)).c_str());
		std::terminate();
	}

	concurrentCount.fetch_add(1u, std::memory_order_release);

	// Matrices do not duplicate data on creation, still using NVDecode frame
	cv::Mat input(height, width, CV_8UC1, myFrame);

	cv::Mat lines;
	cv::Mat edges(input.rows, input.cols, CV_8UC1);
	cv::Canny(input, edges, ocvParams.CannyLow.load(std::memory_order_relaxed),
			  ocvParams.CannyHigh.load(std::memory_order_relaxed));

	cv::HoughLinesP(edges, lines, 1, 0.017, ocvParams.ThresholdHough.load(std::memory_order_relaxed),
					ocvParams.MinLineLength.load(std::memory_order_relaxed),
					ocvParams.MaxLineGap.load(std::memory_order_relaxed));

	// Lines are extracted, we no longer use NVDecode data, Unlock frame is thread safe
	decoder->UnlockFrame(&myFrame, 1);

	SignalFrameSlotReady();

	if(lines.rows > 0) {
		SatLib_Comet_SetInteger(srfWidthElt, width);
		SatLib_Comet_SetInteger(srfHeightElt, height);
		SatLib_Buffer outbuf{(char*)lines.data, (uint32_t)(lines.dataend - lines.datastart)};
		SatLib_Comet_SetInteger(lineCountElt, lines.rows);
		SatLib_Comet_SetBuffer(linesElt, outbuf);

		SatLib_Nebula_SendComet("lines", outLinesComet);
	}

	// Can be wrong (not thread safe), whatever
	unsigned conCount = concurrentCount.load(std::memory_order_acquire);
	if(conCount > concurrentMax) concurrentMax = conCount;
	concurrentCount.fetch_sub(1, std::memory_order_release);

	/*cv::Mat lineImage(input.rows, input.cols, CV_8UC1);
	memset(lineImage.data, 0, lineImage.dataend - lineImage.datastart);
	for(int i = 0; i < lines.rows; ++i) {
		cv::line(lineImage, cv::Point(lines.at<int>(i, 0), lines.at<int>(i, 1)), cv::Point(lines.at<int>(i, 2), lines.at<int>(i, 3)), cv::Scalar(255), 1);
	}
	cv::Mat result;
	cv::addWeighted(input, 0.8, lineImage, 1, 0, result);

	cv::imshow("Result", result);
	cv::waitKey(0);*/
}

void processThread() {
	std::unique_lock<std::mutex> lk(frameMutex, std::defer_lock);
	while(processingFrames.load(std::memory_order_relaxed)) {
		lk.lock();
		frameCV.wait_for(lk, std::chrono::milliseconds(100));
		lk.unlock();

		const uint8_t* frame;
		unsigned long frameSize = 0;
		{
			std::lock_guard<std::mutex> lk(frameQueueMutex);
			unsigned frameSlot = 0;
			while(frameSlot < inFrames.size() && !inFrames[frameSlot]) ++frameSlot;
			if(frameSlot == inFrames.size()) continue;	// Anti spurious wakeup

			frame = inFrames[frameSlot];
			frameSize = inFrameSizes[frameSlot];
			inFrames[frameSlot] = nullptr;
		}
		processFrame(frame, frameSize);
	}
}

void CALLBACK_CONV frameReceived(void* ctx) {
	sbcCometID cid;
	sbcEltID eid;
	if(SatLib_Topic_GetComet(itid, &cid) != 0) return;
	SatLib_Comet_Get(cid, "Frame", &eid);

	SatLib_Buffer buf;
	SatLib_Comet_AsBuffer(eid, &buf);
	SatLib_Comet_Delete(cid);		// Does not delete underlying buffer

	const uint8_t* expected = nullptr;
	int frameSlot = 0;
	{
		std::lock_guard<std::mutex> lk(frameQueueMutex);
		while(frameSlot < inFrames.size() && inFrames[frameSlot]) ++frameSlot;
		if(frameSlot == inFrames.size()) {	// No place for the frame, we have to drop it
			free((void*)buf.data);
			logCB("Dropped a frame");
			return;
		}
		inFrames[frameSlot] = reinterpret_cast<const uint8_t*>(buf.data);
		inFrameSizes[frameSlot] = buf.size;
	}

	frameCV.notify_one();
}

void updateParameters(void* ctx) {
	sbcCometID cid;
	sbcEltID eid;
	int32_t iv;
	double dv;

	SatLib_Topic_GetComet(tptid, &cid);

	SatLib_Comet_Get(cid, "CannyLow", &eid);
	SatLib_Comet_AsInteger(eid, &iv);
	ocvParams.CannyLow.store(iv, std::memory_order_relaxed);
	SatLib_Comet_Get(cid, "CannyHigh", &eid);
	SatLib_Comet_AsInteger(eid, &iv);
	ocvParams.CannyHigh.store(iv, std::memory_order_relaxed);
	SatLib_Comet_Get(cid, "ThresholdHough", &eid);
	SatLib_Comet_AsInteger(eid, &iv);
	ocvParams.ThresholdHough.store(iv, std::memory_order_relaxed);
	SatLib_Comet_Get(cid, "MinLineLength", &eid);
	SatLib_Comet_AsDecimal(eid, &dv);
	ocvParams.MinLineLength.store(dv, std::memory_order_relaxed);
	SatLib_Comet_Get(cid, "MaxLineGap", &eid);
	SatLib_Comet_AsDecimal(eid, &dv);
	ocvParams.MaxLineGap.store(dv, std::memory_order_relaxed);

	logCB("Received parameters");

	SatLib_Comet_Delete(cid);
}

int main() {
	using namespace std::chrono_literals;

	SatLib_Logger_RegisterCallback(logCB, SATLIB_LOGMASK_ALL);
	SatLib_SetDefinitionLoader(defLoadCB);
	SatLib_SetResourceLoader(resLoadCB);

	SatLib_Comet_InitModule();
	SatLib_Nebula_InitModule();
	SatLib_Engine_InitModule("OCVTaskSat");

	while(SatLib_Engine_IsLocalRunning() == 0) {
		logCB("Waiting for engine connection...");
		std::this_thread::sleep_for(2s);
	}

	IGC_Satellite self;
	if(IGC_Satellite_GetLocal("OCVTaskSat", &self) != 0) {
		IGC_Satellite_Deploy("starburst/OCVTaskSat");
		checkAPI(IGC_Satellite_GetLocal("OCVTaskSat", &self));
	}

	auto startFuture = startPromise.get_future();
	auto stopFuture = stopPromise.get_future();

	IGC_Component ocvTaskCo;
	IGC_Component_Future ocvTaskCoFuture;
	SatLib_Component_Setup("starburst/OCVTaskLines", actionCB, nullptr, &ocvTaskCoFuture);
	IGC_Component_GetByFields(ocvTaskCoFuture.name, ocvTaskCoFuture.version, &ocvTaskCo);

	/* Find our session */
	IGC_Session session;
	while(int result = IGC_Session_FindForSat(self.id, &session) > 0) {
		checkAPI(result);
		logCB("Waiting for session to run...");
		std::this_thread::sleep_for(2s);
	}
	checkAPI(SatLib_Engine_JoinSession(session.id, session.engineAddress));

	/* Wait for Start of OCVTaskLines Component*/
	startFuture.wait();

	IGC_Pairing ocvTaskSp{0};
	checkAPI(SatLib_Component_GetSessionPairing(ocvTaskCo.id, &ocvTaskSp) == 0);

	/* Extract parameters from pairing for ease of access */
	std::unordered_map<std::string, IGC_Component_ParamValue> parameters;
	for(unsigned int i = 0; i < ocvTaskSp.paramCount; ++i) {
		parameters.emplace(ocvTaskSp.paramNames[i], ocvTaskSp.paramValues[i]);
	}

	/* Topic ocvTaskLinesParams contains task parameter updates */
	SatLib_Topic_Subscribe("ocvTaskLinesParams", SATLIB_TOPIC_STREAM, true, updateParameters, nullptr, &tptid);

	/* Topic videoFrame contains the video frames to be analysed */
	SatLib_Topic_Subscribe("videoFrame", SATLIB_TOPIC_STREAM, true, frameReceived, nullptr, &itid);

	/* Setup output Comet for lines data */
	checkAPI(SatLib_Comet_Load("starburst/Lines", &outLinesComet));
	SatLib_Comet_Get(outLinesComet, "Lines", &linesElt);
	SatLib_Comet_Get(outLinesComet, "LineCount", &lineCountElt);
	SatLib_Comet_Get(outLinesComet, "SrfWidth", &srfWidthElt);
	SatLib_Comet_Get(outLinesComet, "SrfHeight", &srfHeightElt);

	/* Setup output Comet for processing slot readyness signal */
	checkAPI(SatLib_Comet_Load("starburst/FrameSlotReady", &frameSlotReadyComet));

	InitializeDecoder();

	unsigned threadCount = parameters.at("ThreadCount").valueUnion.int_;
	if(threadCount == 0) threadCount = std::thread::hardware_concurrency();
	std::cout << "Running tasks on " << threadCount << " threads" << std::endl;
	inFrames.resize(threadCount, nullptr);
	inFrameSizes.resize(threadCount, 0);

	auto processingThreads = std::vector<std::thread>();

	SatLib_Component_ReportState(ocvTaskCo.id, SATLIB_COMPONENT_STARTED);

	for(int i = 0; i < threadCount; ++i) {
		processingThreads.emplace_back(processThread);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	for(int i = 0; i < threadCount; ++i) {
		SignalFrameSlotReady();
	}


	while(stopFuture.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
		std::cout << "Active threads: " << concurrentMax << std::endl;
	}

	processingFrames.store(false, std::memory_order_relaxed);
	for(auto& thread : processingThreads) {
		if(thread.joinable()) thread.join();
	}

	DestroyDecoder();

	SatLib_Component_ReportState(ocvTaskCo.id, SATLIB_COMPONENT_STOPPED);

	SatLib_Engine_CloseModule();
	SatLib_Nebula_CloseModule();
	SatLib_Comet_CloseModule();

	return 0;
}
