#include <atomic>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

template<class T>
class AudioHandler
{
public:
	AudioHandler(std::string outputDevice, unsigned int sampleRate = 44100, unsigned int channels = 1, unsigned int blocks = 8, unsigned int blockSamples = 512)
	{
		create(outputDevice, sampleRate, channels, blocks, blockSamples);
	}

	~AudioHandler()
	{
		destroy();
	}

	bool create(std::string outputDevice, unsigned int sampleRate = 44100, unsigned int channels = 1, unsigned int blocks = 8, unsigned int blockSamples = 512)
	{
		isReady = false;
		sampleRate = sampleRate;
		countBlocks = blocks;
		freeBlocks = countBlocks;
		currentBlock = 0;
		blockMemoryPtr = nullptr;
		waveHeaderPtr = nullptr;

		actionPtr = nullptr;

		std::vector<std::string> devices = enumerate();
		auto device = std::find(devices.begin(), devices.end(), outputDevice);
		if (device != devices.end())
		{
			int deviceId = distance(devices.begin(), device);
			WAVEFORMATEX waveFormat;
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nSamplesPerSec = sampleRate;
			waveFormat.wBitsPerSample = sizeof(T) * 8;
			waveFormat.nChannels = channels;
			waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
			waveFormat.cbSize = 0;

			if (waveOutOpen(&hardwareDevices, deviceId, &waveFormat, (DWORD_PTR)waveOutProcWrap, (DWORD_PTR)this, CALLBACK_FUNCTION) != S_OK)
				return destroy();
		}

		blockMemoryPtr = new T[countBlocks * blockSamples];
		if (blockMemoryPtr == nullptr)
			return destroy();
		ZeroMemory(blockMemoryPtr, sizeof(T) * countBlocks * blockSamples);

		waveHeaderPtr = new WAVEHDR[countBlocks];
		if (waveHeaderPtr == nullptr)
			return destroy();
		ZeroMemory(waveHeaderPtr, sizeof(WAVEHDR) * countBlocks);

		for (unsigned int n = 0; n < countBlocks; n++)
		{
			waveHeaderPtr[n].dwBufferLength = blockSamples * sizeof(T);
			waveHeaderPtr[n].lpData = (LPSTR)(blockMemoryPtr + (n * blockSamples));
		}

		isReady = true;
		thread = std::thread(&AudioHandler::mainThread, this);
		std::unique_lock<std::mutex> lm(notZeroBlock);
		notBlockZero.notify_one();
		return true;
	}

	bool destroy()
	{
		return false;
	}

	void stop()
	{
		isReady = false;
		thread.join();
	}

	virtual double userProcess(double time)
	{
		//todo
		return 0.0;
	}

	double getTime()
	{
		return globalTimer;
	}

	static std::vector<std::string>  enumerate()
	{
		int deviceCount = waveOutGetNumDevs();
		std::vector<std::string> devices;
		WAVEOUTCAPS woc;
		for (int n = 0; n < deviceCount; n++) {
			if (waveOutGetDevCaps(n, &woc, sizeof(WAVEOUTCAPS)) == S_OK) {
				devices.push_back((std::string)woc.szPname);
			}
		}
		return devices;
	}

	void setUserFunction(double(*function)(double))
	{
		actionPtr = function;
	}

	double clip(double sample, double max)
	{
		if (sample >= 0.0)
			return fmin(sample, max);
		else
			return fmax(sample, -max);
	}


private:
	double(*actionPtr)(double);

	unsigned int sampleRate;
	unsigned int channels;
	unsigned int countBlocks;
	unsigned int blocksamples;
	unsigned int currentBlock;

	T* blockMemoryPtr;
	WAVEHDR *waveHeaderPtr;
	HWAVEOUT hardwareDevices;

	std::thread thread;
	std::atomic<bool> isReady;
	std::atomic<unsigned int> freeBlocks;
	std::condition_variable notBlockZero;
	std::mutex notZeroBlock;

	std::atomic<double> globalTimer;

	void waveOutProc(HWAVEOUT waveOut, UINT status, DWORD param, DWORD param2)
	{
		if (status != WOM_DONE)
			return;
		freeBlocks++;
		std::unique_lock<std::mutex> lm(notZeroBlock);
		notBlockZero.notify_one();
	}

	static void CALLBACK waveOutProcWrap(HWAVEOUT waveOut, UINT status, DWORD instance, DWORD param, DWORD param2)
	{
		((AudioHandler*)instance)->waveOutProc(waveOut, status, param, param2);
	}


	void mainThread()
	{
		globalTimer = 0.0;
		double timeStep = 1.0 / (double)sampleRate;

		double maxSample = (double)pow(2, (sizeof(double) * 8) - 1) - 1;;
		T previousSample = 0;

		while (isReady)
		{
			if (freeBlocks == 0)
			{
				std::unique_lock<std::mutex> lm(notZeroBlock);
				notBlockZero.wait(lm);
			}

			freeBlocks--;

			if (waveHeaderPtr[currentBlock].dwFlags & WHDR_PREPARED)
				waveOutUnprepareHeader(hardwareDevices, &waveHeaderPtr[currentBlock], sizeof(WAVEHDR));

			T newSample = 0;
			int newCurrentBlock = currentBlock * blocksamples;

			for (unsigned int n = 0; n < blocksamples; n++)
			{
				if (actionPtr == nullptr)
					newSample = (double)(clip(userProcess(globalTimer), 1.0) * maxSample);
				else
					newSample = (double)(clip(actionPtr(globalTimer), 1.0) * maxSample);

				blockMemoryPtr[newCurrentBlock + n] = newSample;
				previousSample = newSample;
				globalTimer = globalTimer + timeStep;
			}

			waveOutPrepareHeader(hardwareDevices, &waveHeaderPtr[currentBlock], sizeof(WAVEHDR));
			waveOutWrite(hardwareDevices, &waveHeaderPtr[currentBlock], sizeof(WAVEHDR));
			currentBlock++;
			currentBlock %= countBlocks;
		}
	}
};
