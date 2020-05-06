#ifndef WWISE_GDNATIVE_H
#define WWISE_GDNATIVE_H

#include <Godot.hpp>
#include <GodotGlobal.hpp>
#include <Node.hpp>
#include <Object.hpp>
#include <Spatial.hpp>

#include "AK/SoundEngine/Win32/AkDefaultIOHookBlocking.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h> 
#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/AkModule.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#include <AK/SoundEngine/Common/AkQueryParameters.h>

#ifndef AK_OPTIMIZED
#include <AK/Comm/AkCommunication.h>
#endif // AK_OPTIMIZED

namespace godot
{

class Wwise : public Node 
{
	GODOT_CLASS(Wwise, Node)

private:
	static bool checkError(AKRESULT result, const char* function, const char* file, int line);
	#define ERROR_CHECK(result) checkError(result, __FUNCTION__, __FILE__, __LINE__)

	enum class VectorType 
	{ 
		position, 
		forward, 
		up
	};

	bool initialiseWwiseSystems();
	bool shutdownWwiseSystems();
	AkVector getAkVector(const Transform t, VectorType type);
	CAkDefaultIOHookBlocking g_lowLevelIO;

public:
	 Wwise();

	~Wwise();

	void _init(); 

	static void _register_methods();

	void _process(float delta);
	
	bool setBasePath(String basePath);
	bool loadBank(String bankName);
	bool unloadBank(String bankName);

	bool registerListener();
	bool setListenerPosition(Object* gameObject);

	bool registerGameObj(Object* gameObject, String gameObjectName);

	bool set3DPosition(Object* gameObject);

	int postEvent(String eventName, Object* gameObject);
	bool stopEvent(int playingID, int fadeTime, int interpolation);

	bool setSwitch(String switchGroup, String switchState, Object* gameObject);
	bool setState(String stateGroup, String stateValue);

	float getRTPCValue(String rtpcName, Object* gameObject);
	bool setRTPCValue(String rtpcName, float rtpcValue, Object* gameObject);
};

}

#endif