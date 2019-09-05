#include <iostream>
#include <fstream>
#include <future>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <cassert>

#include "NvEncoder/NvEncoderCuda.h"

#include <comet.h>
#include <nebulanetwork.h>
#include <engine.h>

constexpr unsigned int width = 1280u, height = 720u;

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
	std::cout << "Received Action " << ((action == SATLIB_COMPONENT_START) ? "START" : "STOP") << std::endl;
	if(action == SATLIB_COMPONENT_START)
		startPromise.set_value();
	else if(action == SATLIB_COMPONENT_STOP)
		stopPromise.set_value();
}

IGC_Session createSession(const char* satID) {
	IGC_Session session;
	IGC_Pairing sp;
	IGC_Satellite taskSat;
	IGC_Component taskCo, clientCo;
	IGC_Template tmpl;

	IGC_Session* sessions;
	uint32_t sessionCount;
	IGC_Session_ListForSat(satID, &sessions, &sessionCount);
	IGC_Template_GetByFields("OCVTemplate", &tmpl);

	for(unsigned int i = 0; i < sessionCount; ++i) {
		if(std::string(sessions[i].templateID) == tmpl.id) return sessions[i];
	}


	if(IGC_Component_GetByFields("OCVTask", "1.0", &taskCo) != 0) {
		throw std::runtime_error("Error getting component OCVTask v1.0");
	}
	if(IGC_Satellite_GetLocal("OCVSat", &taskSat) != 0) {
		throw std::runtime_error("Error getting satellite OCVSat");
	}

	IGC_Component_GetByFields("OCVClient", "1.0", &clientCo);


	const char* paramNames[2] = {"CaptureWidth", "CaptureHeight"};
	IGC_Component_ParamValue paramValues[2];
	paramValues[0].type = IGC_COMPONENT_PARAMTYPE_INT;
	paramValues[0].valueUnion.int_ = width;
	paramValues[1].type = IGC_COMPONENT_PARAMTYPE_INT;
	paramValues[1].valueUnion.int_ = height;
	IGC_Session_Create(tmpl.id, "OCVSession", &session);
	IGC_Pairing_Register(session.id, taskCo.id, taskSat.id, paramNames, paramValues, 2, &sp);
	IGC_Pairing_Register(session.id, clientCo.id, satID, paramNames, paramValues, 2, &sp);

	return session;
}

CUdevice encodeDevice = 0;
CUcontext encodeCtx = nullptr;
NvEncoder* encoder = nullptr;
void InitializeEncoder(uint32_t width, uint32_t height) {
	cuInit(0);

	cuDeviceGet(&encodeDevice, 0);
	char szDeviceName[80];
	cuDeviceGetName(szDeviceName, sizeof(szDeviceName), encodeDevice);
	std::cout << "GPU in use: " << szDeviceName << std::endl;

	if(cuCtxCreate(&encodeCtx, 0, encodeDevice) != CUDA_SUCCESS) {
		logCB("Could not create CUDA context");
		return;
	}
	if((encoder = new NvEncoderCuda(encodeCtx, width, height, NV_ENC_BUFFER_FORMAT_NV12, 0u)) == nullptr) {
		logCB("Error creating encoder");
		return;
	}

	NV_ENC_INITIALIZE_PARAMS initParams = {NV_ENC_INITIALIZE_PARAMS_VER};
	NV_ENC_CONFIG encodeConfig = {NV_ENC_CONFIG_VER};
	initParams.encodeConfig = &encodeConfig;
	encoder->CreateDefaultEncoderParams(&initParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID);

	encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
	encodeConfig.frameIntervalP = 1;
	encodeConfig.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
	encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;

	encodeConfig.rcParams.averageBitRate = 1500000;	// 1.5Mbps
	encodeConfig.rcParams.vbvBufferSize = (encodeConfig.rcParams.averageBitRate * initParams.frameRateDen / initParams.frameRateNum) * 5;
	encodeConfig.rcParams.maxBitRate = encodeConfig.rcParams.averageBitRate;
	encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;

	encoder->CreateEncoder(&initParams);
}

// Calling multiple times is safe
void DestroyEncoder() {
	delete encoder;
	encoder = nullptr;

	if(encodeCtx != nullptr) {
		if(cuCtxDestroy(encodeCtx) != CUDA_SUCCESS) {
			logCB("Could not destroy CUDA context");
		}
		encodeCtx = nullptr;
	}
}

sbcCometID inFrameComet, taskParamsComet;
sbcEltID inFrameBuffer;


std::atomic<const char*> linesData;
std::atomic<int> linesCount;
std::atomic<int> receivedLines = 0;

std::mutex linesMutex;
std::condition_variable linesCV;

void drawLines() {
	static std::ofstream file("outLines.txt");
	static int i = 0;

	const int* data = (const int*)linesData.exchange(nullptr, std::memory_order_relaxed);
	auto count = linesCount.load(std::memory_order_relaxed);


	if(i == 0) {
		file << "Line count: " << count << std::endl;
		for(int i = 0; i < count; ++i) {
			file << "Line x0:" << data[4*i +0] << ", y0:" << data[4*i +1] << "	x1:" << data[4*i +2] << ", y1:" << data[4*i +3] << std::endl;
		}
	}

	i++;
}

sbcTopicID ltid;
void linesReceived(void* context) {
	sbcCometID cid;
	sbcEltID elt;

	SatLib_Topic_GetComet(ltid, &cid);
	SatLib_Comet_Get(cid, "Lines", &elt);
	SatLib_Buffer buf;
	SatLib_Comet_AsBuffer(elt, &buf);

	SatLib_Comet_Get(cid, "LineCount", &elt);
	int32_t lc;
	SatLib_Comet_AsInteger(elt, &lc);
	SatLib_Comet_Delete(cid);

	linesCount.store(lc, std::memory_order_relaxed);
	linesData.store(buf.data, std::memory_order_release);

	linesCV.notify_one();
}

void setupTaskParameters() {
	sbcEltID eid;
	SatLib_Comet_Get(taskParamsComet, "CannyLow", &eid);
	SatLib_Comet_SetInteger(eid, 100);
	SatLib_Comet_Get(taskParamsComet, "CannyHigh", &eid);
	SatLib_Comet_SetInteger(eid, 200);
	SatLib_Comet_Get(taskParamsComet, "ThresholdHough", &eid);
	SatLib_Comet_SetInteger(eid, 5);
	SatLib_Comet_Get(taskParamsComet, "MinLineLength", &eid);
	SatLib_Comet_SetDecimal(eid, 150.0);
	SatLib_Comet_Get(taskParamsComet, "MaxLineGap", &eid);
	SatLib_Comet_SetDecimal(eid, 4.0);
}

int main() {
	using namespace std::chrono_literals;

	SatLib_Logger_RegisterCallback(logCB, SATLIB_LOGMASK_ALL);
	SatLib_SetDefinitionLoader(defLoadCB);
	SatLib_SetResourceLoader(resLoadCB);

	SatLib_Comet_InitModule();
	SatLib_Nebula_InitModule();
	SatLib_Engine_InitModule("OCVClientSat");

	IGC_Satellite self;
	if(IGC_Satellite_GetLocal("OCVClientSat", &self) != 0) {
		IGC_Satellite_Deploy("starburst/OCVClientSat");
		assert(IGC_Satellite_GetLocal("OCVClientSat", &self) == 0);
	}
	IGC_Template tmpl;
	if(IGC_Template_GetByFields("OCVTemplate", &tmpl) != 0) {
		IGC_Template_Deploy("starburst/OCVTemplate");
		assert(IGC_Template_GetByFields("OCVTemplate", &tmpl) == 0);
	}

	auto startFuture = startPromise.get_future();
	auto stopFuture = stopPromise.get_future();
	IGC_Component ocvClientCo;

	assert(SatLib_Component_Setup("starburst/OCVClient", actionCB, nullptr, &ocvClientCo) == 0);

	IGC_Session session = createSession(self.id);
	assert(SatLib_Engine_JoinSession(session.id, session.engineAddress) == 0);

	IGC_Pairing ocvClientSp;
	assert(SatLib_Component_GetSessionPairing(ocvClientCo.id, &ocvClientSp) == 0);

	std::unordered_map<std::string, IGC_Component_ParamValue> parameters;
	for(unsigned int i = 0; i < ocvClientSp.paramCount; ++i) {
		parameters.emplace(ocvClientSp.paramNames[i], ocvClientSp.paramValues[i]);
	}


	SatLib_Topic_Subscribe("outLines", SATLIB_TOPIC_STREAM, true, linesReceived, nullptr, &ltid);

	// Setup output Comet
	assert(SatLib_Comet_Load("starburst/InFrame", &inFrameComet) == 0);
	assert(SatLib_Comet_Load("starburst/TaskParameters", &taskParamsComet) == 0);
	SatLib_Comet_Get(inFrameComet, "Frame", &inFrameBuffer);
	setupTaskParameters();

	if(session.status == IGC_SESSIONSTATUS_STOPPED) {
		IGC_Session_Start(session.id, session.engineAddress);
	}

	startFuture.wait();

	InitializeEncoder((uint32_t)parameters.at("CaptureWidth").valueUnion.int_, (uint32_t)parameters.at("CaptureHeight").valueUnion.int_);
	SatLib_Nebula_SendComet("taskParameters", taskParamsComet);

	SatLib_Component_ReportState(ocvClientCo.id, SATLIB_COMPONENT_STARTED);

	bool drawingLines = true;
	auto drawThread = std::thread([&drawingLines]() {
		std::unique_lock<std::mutex> lk(linesMutex);
		while(drawingLines) {
			linesCV.wait_for(lk, 50ms);
			if(linesData.load(std::memory_order_acquire) != nullptr) {
				receivedLines.fetch_add(1, std::memory_order_relaxed);
				drawLines();
			}
		}
									 });

	uint32_t frameSize = encoder->GetFrameSize(), nFrame = 0;
	char* inputBuffer = (char*)malloc(frameSize);

	std::ifstream infile("out.yuv", std::ios::binary);
	if(!infile.good()) {
		logCB("Impossible to open input");
		return -1;
	}

	// NVENC work
	NV_ENC_PIC_PARAMS picParams = {NV_ENC_PIC_PARAMS_VER};
	picParams.encodePicFlags = 0;

	int framesSent = 0;
	std::streamsize nRead = 0;
	do {
		std::vector<std::vector<uint8_t>> vPacket;
		nRead = infile.read(inputBuffer, frameSize).gcount();


		if(nRead == frameSize) {
			const NvEncInputFrame* encInput = encoder->GetNextInputFrame();
			NvEncoderCuda::CopyToDeviceFrame(encodeCtx,
											 inputBuffer,
											 0,
											 (CUdeviceptr)encInput->inputPtr,
											 encInput->pitch,
											 encoder->GetEncodeWidth(),
											 encoder->GetEncodeHeight(),
											 CU_MEMORYTYPE_HOST,
											 encInput->bufferFormat,
											 encInput->chromaOffsets,
											 encInput->numChromaPlanes);

			encoder->EncodeFrame(vPacket, &picParams);
		} else {
			encoder->EndEncode(vPacket);
		}
		nFrame += (uint32_t)vPacket.size();
		std::cout << "Read " << vPacket.size() << " frames:" << std::endl;
		for(const auto& frame : vPacket) {
			std::cout << "	Size: " << frame.size() << std::endl;

			SatLib_Comet_SetBuffer(inFrameBuffer, {(const char*)frame.data(), (unsigned long)frame.size()});
			SatLib_Nebula_SendComet("inFrame", inFrameComet);
			framesSent++;

			while(receivedLines.load(std::memory_order_relaxed) < framesSent) {
				std::this_thread::sleep_for(50ms);
			}
		}
	} while(nRead == frameSize);

	IGC_Session_Stop(session.id, session.engineAddress);

	stopFuture.wait();
	drawingLines = false;
	if(drawThread.joinable()) drawThread.join();

	DestroyEncoder();
	infile.close();
	free(inputBuffer);

	SatLib_Component_ReportState(ocvClientCo.id, SATLIB_COMPONENT_STOPPED);

	SatLib_Engine_CloseModule();
	SatLib_Nebula_CloseModule();
	SatLib_Comet_CloseModule();

	return 0;
}
