#include "AudioHandler.h"

#define OSCILATOR_SINE 0
#define OSCILATOR_SQUARE 1
#define OSCILATOR_TRIANGLE 2
#define OSCILATOR_SAW_ANALOG 3
#define OSCILATOR_SAW_DIGITAL 4
#define OSCILATOR_NOISE 5
#define PI 3.1415

struct ADSR
{
	double attackTime;
	double decayTime;
	double sustainAmplitude;
	double releaseTime;
	double startAmplitude;
	double triggerOffTime;
	double triggerOnTime;
	bool isNoteOn;

	ADSR()
	{
		attackTime = 0.10;
		decayTime = 0.01;
		startAmplitude = 1.0;
		sustainAmplitude = 0.8;
		releaseTime = 0.20;
		isNoteOn = false;
		triggerOffTime = 0.0;
		triggerOnTime = 0.0;
	}

	void NoteOn(double timeOn)
	{
		triggerOnTime = timeOn;
		isNoteOn = true;
	}

	void NoteOff(double timeOff)
	{
		triggerOffTime = timeOff;
		isNoteOn = false;
	}

	double GetAmplitude(double time)
	{
		double amplitude = 0.0;
		double lifeTime = time - triggerOnTime;

		if (isNoteOn)
		{
			if (lifeTime <= attackTime)
			{
				amplitude = (lifeTime / attackTime) * startAmplitude;
			}

			if (lifeTime > attackTime && lifeTime <= (attackTime + decayTime))
			{
				amplitude = ((lifeTime - attackTime) / decayTime) * (sustainAmplitude - startAmplitude) + startAmplitude;
			}

			if (lifeTime > (attackTime + decayTime))
			{
				amplitude = sustainAmplitude;
			}
		}
		else {
			amplitude = ((time - triggerOffTime) / releaseTime) * (0.0 - sustainAmplitude) + sustainAmplitude;
		}

		if (amplitude <= 0.0001)
			amplitude = 0.0;

		return amplitude;
	}
};

double hzToCurcVelocity(double hz)
{
	return hz * 2.0 * PI;
}

double oscilator(double hz, double time, int type = OSCILATOR_SINE)
{
	switch (type)
	{
	case OSCILATOR_SINE:
		return sin(hzToCurcVelocity(hz) * time);
	case OSCILATOR_SQUARE:
		return sin(hzToCurcVelocity(hz) * time) > 0 ? 1.0 : -1.0;
	case OSCILATOR_TRIANGLE:
		return asin(sin(hzToCurcVelocity(hz) * time)) * (2.0 / PI);
	case OSCILATOR_SAW_ANALOG:
	{
		double dOutput = 0.0;
		for (double n = 1.0; n < 40.0; n++)
			dOutput += (sin(n * hzToCurcVelocity(hz) * time)) / n;

		return dOutput * (2.0 / PI);
	}
	case OSCILATOR_SAW_DIGITAL:
		return (2.0 / PI) * (hz * PI * fmod(time, 1.0 / hz) - (PI / 2.0));
	case OSCILATOR_NOISE:
		return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;
	default:
		return 0.0;
	}
}

// GLOBAL VARS 
std::atomic<double> frequencyOutput = 0.0;
ADSR adsr;
double octaveBaseFrequency = 110.0;
double noteBaseFrequency = pow(2.0, 1.0 / 12.0);

/**
output multipled for quieter tone
*/
double generateSound(double time)
{
	double dOutput = adsr.GetAmplitude(time) *
		(
			+1.0 * oscilator(frequencyOutput * 0.5, time, OSCILATOR_SINE)
			+ 1.0 * oscilator(frequencyOutput, time, OSCILATOR_SAW_ANALOG)
			);

	return dOutput;
}

int main()
{
	std::vector<std::string> devices = AudioHandler<short>::enumerate();

	for (auto d : devices)
	{
		std::cout << "Found Output Device: " << d << std::endl;
	}
	std::cout << "Using Device: " << devices[0] << std::endl;

	std::cout << std::endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << std::endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << std::endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << std::endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << std::endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << std::endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << std::endl << std::endl;


	AudioHandler<short> sound(devices[0], 44100, 1, 8, 512);
	sound.setUserFunction(generateSound);

	int currentKey = -1;
	bool isKeyPressed = false;
	while (1)
	{
		isKeyPressed = false;
		for (int k = 0; k < 16; k++)
		{
			if (GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k])) & 0x8000)
			{
				if (currentKey != k)
				{
					frequencyOutput = octaveBaseFrequency * pow(noteBaseFrequency, k);
					adsr.NoteOn(sound.getTime());
					std::cout << "\rNote On : " << sound.getTime() << "s " << frequencyOutput << "Hz";
					currentKey = k;
				}

				isKeyPressed = true;
			}
		}

		if (!isKeyPressed)
		{
			if (currentKey != -1)
			{
				std::cout << "\rNote Off: " << sound.getTime() << "s                        ";
				adsr.NoteOff(sound.getTime());
				currentKey = -1;
			}
		}
	}

	return 0;
}