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

struct {
	int width, height;
} frameParams;
sbcTopicID tptid, itid;

struct {
	std::atomic<double> MinLineLength = 100.0, MaxLineGap = 10.0;
	std::atomic<int> CannyLow = 100, CannyHigh = 200, ThresholdHough = 5;
} ocvParams;
sbcCometID outLinesComet;
sbcEltID linesElt, lineCountElt;

std::atomic<const uint8_t*> inFrame;
std::atomic<unsigned long> inFrameSize;

void processFrame() {
	static int globalFrameNumber = 0;
	const uint8_t* currentFrame = inFrame.exchange(nullptr, std::memory_order_acq_rel);
	unsigned long currentSize = inFrameSize.load(std::memory_order_relaxed);

	uint8_t** outFrames;
	int outFrameCount = 0;
	if(decoder->Decode(currentFrame, currentSize, &outFrames, &outFrameCount, CUVID_PKT_ENDOFPICTURE, nullptr, globalFrameNumber++) != true) {
		logCB("Error on decode");
		return;
	}

	if(outFrameCount != 1) {
		logCB(("Error received frame count: " + std::to_string(outFrameCount)).c_str());
		return;
	}

	cv::Mat input(frameParams.height, frameParams.width, CV_8UC1, outFrames[0]);

	cv::Mat lines;
	cv::Mat edges(input.rows, input.cols, CV_8UC1);
	cv::Canny(input, edges, ocvParams.CannyLow.load(std::memory_order_relaxed),
			  ocvParams.CannyHigh.load(std::memory_order_relaxed));

	cv::HoughLinesP(edges, lines, 1, 0.017, ocvParams.ThresholdHough.load(std::memory_order_relaxed),
					ocvParams.MinLineLength.load(std::memory_order_relaxed),
					ocvParams.MaxLineGap.load(std::memory_order_relaxed));

	SatLib_Buffer outbuf{(char*)lines.data, (uint32_t)(lines.dataend - lines.datastart)};
	SatLib_Comet_SetInteger(lineCountElt, lines.rows);
	SatLib_Comet_SetBuffer(linesElt, outbuf);

	/*int buf[] = {0, 0, 1024, 1024, 1024, 0, 0, 1024};
	SatLib_Comet_SetInteger(lineCountElt, sizeof(buf) >> 4);
	SatLib_Comet_SetBuffer(linesElt, SatLib_Buffer{(const char*)buf, sizeof(buf)});*/

	SatLib_Nebula_SendComet("outLines", outLinesComet);

	cv::Mat lineImage(input.rows, input.cols, CV_8UC1);
	memset(lineImage.data, 0, lineImage.dataend - lineImage.datastart);
	for(int i = 0; i < lines.rows; ++i) {
		cv::line(lineImage, cv::Point(lines.at<int>(i, 0), lines.at<int>(i, 1)), cv::Point(lines.at<int>(i, 2), lines.at<int>(i, 3)), cv::Scalar(255), 1);
	}
	cv::Mat result;
	cv::addWeighted(input, 0.8, lineImage, 1, 0, result);

	cv::imshow("Result", result);
	cv::waitKey(0);
}

std::mutex frameMutex;
std::condition_variable frameCV;

void CALLBACK_CONV frameReceived(void* ctx) {
	sbcCometID cid;
	sbcEltID eid;
	if(SatLib_Topic_GetComet(itid, &cid) != 0) return;
	SatLib_Comet_Get(cid, "Frame", &eid);

	SatLib_Buffer buf;
	SatLib_Comet_AsBuffer(eid, &buf);
	SatLib_Comet_Delete(cid);		// Does not delete underlying buffer

	const uint8_t* expected = nullptr;
	if(!inFrame.compare_exchange_strong(expected, (const uint8_t*)buf.data, std::memory_order_acq_rel)) {
		free((void*)buf.data);
		logCB("Dropped a frame");
		return;
	}
	inFrameSize.store(buf.size, std::memory_order_relaxed);

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

	SatLib_Comet_Delete(cid);
}

int main() {
	using namespace std::chrono_literals;

	SatLib_Logger_RegisterCallback(logCB, SATLIB_LOGMASK_ALL);
	SatLib_SetDefinitionLoader(defLoadCB);
	SatLib_SetResourceLoader(resLoadCB);

	SatLib_Comet_InitModule();
	SatLib_Nebula_InitModule();
	SatLib_Engine_InitModule("OCVSat");

	while(SatLib_Engine_IsLocalRunning() == 0) {
		logCB("Waiting for engine connection...");
		std::this_thread::sleep_for(1s);
	}

	IGC_Satellite self;
	if(IGC_Satellite_GetLocal("OCVSat", &self) != 0) {
		IGC_Satellite_Deploy("starburst/OCVSat");
		assert(IGC_Satellite_GetLocal("OCVSat", &self) == 0);
	}

	auto startFuture = startPromise.get_future();
	auto stopFuture = stopPromise.get_future();

	IGC_Component ocvTaskCo;
	IGC_Component_Future ocvTaskCoFuture;
	SatLib_Component_Setup("starburst/OCVTask", actionCB, nullptr, &ocvTaskCoFuture);
	IGC_Component_GetByFields(ocvTaskCoFuture.name, ocvTaskCoFuture.version, &ocvTaskCo);
	SatLib_Free(ocvTaskCoFuture.name);
	SatLib_Free(ocvTaskCoFuture.version);

	IGC_Session session;
	while(IGC_Session_FindForSat(self.id, &session) > 0) {
		std::this_thread::sleep_for(1s);
	}
	assert(SatLib_Engine_JoinSession(session.id, session.engineAddress) == 0);

	IGC_Pairing ocvTaskSp;
	assert(SatLib_Component_GetSessionPairing(ocvTaskCo.id, &ocvTaskSp) == 0);

	std::unordered_map<std::string, IGC_Component_ParamValue> parameters;
	for(unsigned int i = 0; i < ocvTaskSp.paramCount; ++i) {
		parameters.emplace(ocvTaskSp.paramNames[i], ocvTaskSp.paramValues[i]);
	}

	SatLib_Topic_Subscribe("taskParameters", SATLIB_TOPIC_STREAM, true, updateParameters, nullptr, &tptid);
	SatLib_Topic_Subscribe("inFrame", SATLIB_TOPIC_STREAM, true, frameReceived, nullptr, &itid);

	/* Setup output Comet */
	assert(SatLib_Comet_Load("starburst/OutLines", &outLinesComet) == 0);
	SatLib_Comet_Get(outLinesComet, "Lines", &linesElt);
	SatLib_Comet_Get(outLinesComet, "LineCount", &lineCountElt);

	startFuture.wait();

	InitializeDecoder();

	SatLib_Component_ReportState(ocvTaskCo.id, SATLIB_COMPONENT_STARTED);

	frameParams.width = (int)parameters.at("CaptureWidth").valueUnion.int_;
	frameParams.height = (int)parameters.at("CaptureHeight").valueUnion.int_;

	/* Starting moonvdec */
	/*assert(mvd_Init(logCB) == 0);
	atexit(mvd_Close);

	mvd_StreamSource src = mvd_GetStreamSource(parameters.at("GamestreamAddress").valueUnion.string);
	assert(src != nullptr);

	assert(mvd_PairStreamSource(src, "2566") == 0);

	const int* ids, *lens;
	const wchar_t* const* names;
	int count = mvd_GetAppList(src, &ids, &names, &lens);
	assert(count >= 0);

	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
	auto appNameW = converter.from_bytes(parameters.at("GamestreamApp").valueUnion.string);
	int appID = -1;
	for(int i = 0; i < count; ++i) {
		if(names[i] == appNameW) {
			appID = ids[i];
			break;
		}
	}
	assert(appID != -1);

	sconfig.width = (int)parameters.at("CaptureWidth").valueUnion.int_;
	sconfig.height = (int)parameters.at("CaptureHeight").valueUnion.int_;
	sconfig.fps = (int)parameters.at("RefreshRate").valueUnion.int_;
	sconfig.bitrate = (int)parameters.at("Bitrate").valueUnion.int_ * 1000;
	sconfig.packetSize = 1024;
	sconfig.streamingRemotely = (int)false;
	sconfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
	sconfig.supportsHevc = (int)false;
	sconfig.enableHdr = (int)false;
	sconfig.hevcBitratePercentageMultiplier = 75;
	sconfig.clientRefreshRateX100 = 5994;

	mvd_LaunchApp(src, appID, &sconfig);
	assert(mvd_StartStream(src, &sconfig, frameCB, nullptr) == 0);*/

	bool processingFrames = true;

	auto processThread = std::thread([&processingFrames]() {
		std::unique_lock<std::mutex> lk(frameMutex);
		while(processingFrames) {
			frameCV.wait_for(lk, 50ms);
			if(inFrame.load(std::memory_order_acquire) != nullptr) {
				processFrame();
			}
		}
									 });

	stopFuture.wait();
	processingFrames = false;
	if(processThread.joinable()) processThread.join();

	DestroyDecoder();

	SatLib_Component_ReportState(ocvTaskCo.id, SATLIB_COMPONENT_STOPPED);

	//mvd_StopStream(src);

	SatLib_Engine_CloseModule();
	SatLib_Nebula_CloseModule();
	SatLib_Comet_CloseModule();

	return 0;
}
