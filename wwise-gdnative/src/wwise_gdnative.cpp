#include "wwise_gdnative.h"

#include <AK/Plugin/AllPluginsFactories.h>

using namespace godot;

Mutex* Wwise::signalDataMutex;
std::vector<SignalData> Wwise::signalDataVector;

namespace AK
{
	void* __cdecl AllocHook(size_t in_size)
	{
		Godot::print("AK::AllocHook called with size: " + String::num_int64(in_size));
		AKASSERT(api);
		return api->godot_alloc(static_cast<int>(in_size));
	}

	void __cdecl FreeHook(void* in_ptr)
	{
		Godot::print("AK::FreeHook called");
		AKASSERT(api);
		api->godot_free(in_ptr);
	}

#if defined(_WIN32)
	void* __cdecl VirtualAllocHook(
		void* in_pMemAddress,
		size_t in_size,
		DWORD in_dwAllocationType,
		DWORD in_dwProtect
	)
	{
		return VirtualAlloc(in_pMemAddress, in_size, in_dwAllocationType, in_dwProtect);
	}
	void __cdecl VirtualFreeHook(
		void* in_pMemAddress,
		size_t in_size,
		DWORD in_dwFreeType
	)
	{
		VirtualFree(in_pMemAddress, in_size, in_dwFreeType);
	}
#endif
}

#ifndef AK_OPTIMIZED
AkMemPoolId g_poolComm = AK_INVALID_POOL_ID;
#define COMM_POOL_SIZE (256 * 1024)
#define COMM_POOL_BLOCK_SIZE (48)
#endif

Wwise::~Wwise() 
{
	shutdownWwiseSystems();

	signalDataMutex->free();
	signalDataMutex = nullptr;

	Godot::print("Wwise has shut down");
}

void Wwise::_register_methods()
{
	register_method("_process", &Wwise::_process);
	register_method("set_base_path", &Wwise::setBasePath);
	register_method("load_bank", &Wwise::loadBank);
	register_method("load_bank_id", &Wwise::loadBankID);
	register_method("unload_bank", &Wwise::unloadBank);
	register_method("unload_bank_id", &Wwise::unloadBankID);
	register_method("register_listener", &Wwise::registerListener);
	register_method("register_game_obj", &Wwise::registerGameObject);
	register_method("set_3d_position", &Wwise::set3DPosition);
	register_method("set_2d_position", &Wwise::set2DPosition);
	register_method("post_event", &Wwise::postEvent);
	register_method("post_event_callback", &Wwise::postEventCallback);
	register_method("post_event_id", &Wwise::postEventID);
	register_method("post_event_id_callback", &Wwise::postEventIDCallback);
	register_method("stop_event", &Wwise::stopEvent);
	register_method("set_switch", &Wwise::setSwitch);
	register_method("set_switch_id", &Wwise::setSwitchID);
	register_method("set_state", &Wwise::setState);
	register_method("set_state_id", &Wwise::setStateID);
	register_method("get_rtpc", &Wwise::getRTPCValue);
	register_method("get_rtpc_id", &Wwise::getRTPCValueID);
	register_method("set_rtpc", &Wwise::setRTPCValue);
	register_method("set_rtpc_id", &Wwise::setRTPCValueID);
	register_method("post_trigger", &Wwise::postTrigger);
	register_method("post_trigger_id", &Wwise::postTriggerID);
	register_method("post_external_source", &Wwise::postExternalSource);
	register_method("post_external_source_id", &Wwise::postExternalSourceID);

	register_signal<Wwise>("audio_marker", "params", GODOT_VARIANT_TYPE_DICTIONARY);
}

void Wwise::_init()
{
	signalDataMutex = Mutex::_new();

	bool initialisationResult = initialiseWwiseSystems();

	if (!initialisationResult)
	{
		ERROR_CHECK(AK_Fail, "Wwise systems initialisation failed!");
	}
	else
	{
		Godot::print("Wwise systems initialisation succeeded");
	}
}

void Wwise::_process(const float delta)
{
	emitSignals();
	ERROR_CHECK(AK::SoundEngine::RenderAudio(), "");
}

void Wwise::eventCallback(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
{
	signalDataMutex->lock();

	SignalData signalData;

	switch (in_eType)
	{
		case AK_Marker:
		{
			signalData.sourceCallbackType = AK_Marker;
			AkMarkerCallbackInfo* markerInfo = static_cast<AkMarkerCallbackInfo*>(in_pCallbackInfo);
			signalData.eventData["uIdentifier"] = static_cast<unsigned int>(markerInfo->uIdentifier);
			signalData.eventData["uPosition"] = static_cast<unsigned int>(markerInfo->uPosition);
			signalData.eventData["strLabel"] = String(markerInfo->strLabel);

			signalDataVector.push_back(signalData);
			break;
		}
		default:
			break;
	}

	signalDataMutex->unlock();
}

void Wwise::emitSignals()
{
	signalDataMutex->lock();

	for (unsigned int signalIndex = 0; signalIndex < signalDataVector.size(); ++signalIndex)
	{
		SignalData* signalData = static_cast<SignalData*>(&signalDataVector[signalIndex]);

		switch (signalData->sourceCallbackType)
		{
		case AK_Marker:
			emit_signal("audio_marker", signalData->eventData);
			break;
		default:
			break;
		}
	}

	if (signalDataVector.size() > 0)
	{
		signalDataVector.clear();
	}

	signalDataMutex->unlock();
}

bool Wwise::initialiseWwiseSystems()
{
	AkMemSettings memSettings;
	AK::MemoryMgr::GetDefaultSettings(memSettings);
	if (!ERROR_CHECK(AK::MemoryMgr::Init(&memSettings), "Memory manager initialisation failed"))
	{
		return false;
	}

	AkStreamMgrSettings stmSettings;
	AK::StreamMgr::GetDefaultSettings(stmSettings);
	if (!AK::StreamMgr::Create(stmSettings))
	{
		return false;
	}

	AkDeviceSettings deviceSettings;
	AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
	if (!ERROR_CHECK(lowLevelIO.Init(deviceSettings), "Low level IO failed"))
	{
		return false;
	}

	AkInitSettings initSettings;
	AkPlatformInitSettings platformInitSettings;
	AK::SoundEngine::GetDefaultInitSettings(initSettings);
	AK::SoundEngine::GetDefaultPlatformInitSettings(platformInitSettings);
	if (!ERROR_CHECK(AK::SoundEngine::Init(&initSettings, &platformInitSettings), "Sound engine initialisation failed"))
	{
		return false;
	}

	AkMusicSettings musicInit;
	AK::MusicEngine::GetDefaultInitSettings(musicInit);
	if (!ERROR_CHECK(AK::MusicEngine::Init(&musicInit), "Music engine initialisation failed"))
	{
		return false;
	}

#ifndef AK_OPTIMIZED
	AkCommSettings settingsComm;
	AK::Comm::GetDefaultInitSettings(settingsComm);
	if (!ERROR_CHECK(AK::Comm::Init(settingsComm), "Comm initialisation failed"))
	{
		return false;
	}
#endif

	return true;
}

bool Wwise::shutdownWwiseSystems()
{
#ifndef AK_OPTIMIZED
	AK::Comm::Term();
#endif

	if (!ERROR_CHECK(AK::SoundEngine::UnregisterAllGameObj(), "Unregister all game obj failed"))
	{
		return false;
	}

	if (!ERROR_CHECK(AK::SoundEngine::ClearBanks(), "Clear banks failed"))
	{
		return false;
	}

	AK::MusicEngine::Term();

	AK::SoundEngine::Term();

	lowLevelIO.Term();

	if (AK::IAkStreamMgr::Get())
	{
		AK::IAkStreamMgr::Get()->Destroy();
	}

	AK::MemoryMgr::Term();

	return true;
}

bool Wwise::setBasePath(const String basePath)
{
	AKASSERT(!basePath.empty());

	AkOSChar* basePathOsString = nullptr;

	const wchar_t* basePathChar = basePath.unicode_str();
	CONVERT_WIDE_TO_OSCHAR(basePathChar, basePathOsString);
	AKASSERT(basePathOsString);

	return ERROR_CHECK(lowLevelIO.SetBasePath(basePathOsString), basePath);
}

bool Wwise::loadBank(const String bankName)
{
	AkBankID bankID;
	AKASSERT(!bankName.empty());

	return ERROR_CHECK(AK::SoundEngine::LoadBank(bankName.unicode_str(), bankID), bankName);
}

bool Wwise::loadBankID(const unsigned int bankID)
{
	return ERROR_CHECK(AK::SoundEngine::LoadBank(bankID), "ID " + String::num_int64(bankID));
}

bool Wwise::unloadBank(const String bankName)
{
	AKASSERT(!bankName.empty());

	return ERROR_CHECK(AK::SoundEngine::UnloadBank(bankName.unicode_str(), NULL), bankName);
}

bool Wwise::unloadBankID(const unsigned int bankID)
{
	return ERROR_CHECK(AK::SoundEngine::UnloadBank(bankID, NULL), "ID " + String::num_int64(bankID) + " failed");
}

bool Wwise::registerListener(const Object* gameObject)
{
	AKASSERT(gameObject);

	const AkGameObjectID listener = static_cast<AkGameObjectID>(gameObject->get_instance_id());

	if (!ERROR_CHECK(AK::SoundEngine::RegisterGameObj(listener, "Default Listener"), "ID " + String::num_int64(listener)))
	{
		return false;
	}

	if (!ERROR_CHECK(AK::SoundEngine::SetDefaultListeners(&listener, 1), "ID " + String::num_int64(listener)))
	{
		return false;
	}

	return true;
}

bool Wwise::registerGameObject(const Object* gameObject, const String gameObjectName)
{
	AKASSERT(gameObject);
	AKASSERT(!gameObjectName.empty());

	return ERROR_CHECK(AK::SoundEngine::RegisterGameObj(static_cast<AkGameObjectID>(gameObject->get_instance_id()), 
						gameObjectName.alloc_c_string()), gameObjectName);
}

bool Wwise::set3DPosition(const Object* gameObject, const Transform transform)
{
	AKASSERT(gameObject);

	AkSoundPosition soundPos;

	AkVector position;	GetAkVector(transform, position, VectorType::POSITION);
	AkVector forward;	GetAkVector(transform, forward, VectorType::FORWARD);
	AkVector up;		GetAkVector(transform, up, VectorType::UP);

	soundPos.Set(position, forward, up);

	return ERROR_CHECK(AK::SoundEngine::SetPosition(static_cast<AkGameObjectID>(gameObject->get_instance_id()), soundPos),
						"Game object ID " + String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::set2DPosition(const Object* gameObject, const Transform2D transform2D, const float zDepth)
{
	AKASSERT(gameObject);

	AkSoundPosition soundPos;

	Vector2 origin = transform2D.get_origin();

	Vector3 position = Vector3(origin.x * 0.1f, -origin.y * 0.1f, zDepth);
	Vector3 forward = Vector3(transform2D.elements[1].x, 0, transform2D.elements[1].y).normalized();
	Vector3 up = Vector3(0, 1, 0);

	AkVector akPosition;	Vector3ToAkVector(position, akPosition);
	AkVector akForward;		Vector3ToAkVector(forward, akForward);
	AkVector akUp;			Vector3ToAkVector(up, akUp);

	soundPos.Set(akPosition, akForward, akUp);

	return ERROR_CHECK(AK::SoundEngine::SetPosition(static_cast<AkGameObjectID>(gameObject->get_instance_id()), soundPos),
						"Game object ID " + String::num_int64(gameObject->get_instance_id()));
}

unsigned int Wwise::postEvent(const String eventName, const Object* gameObject)
{
	AKASSERT(!eventName.empty());
	AKASSERT(gameObject);

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventName.unicode_str(), static_cast<AkGameObjectID>(gameObject->get_instance_id()));

	if (playingID == AK_INVALID_PLAYING_ID) 
	{
		ERROR_CHECK(AK_InvalidID, eventName);
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postEventCallback(const String eventName, const unsigned int flags, const Object* gameObject)
{
	AKASSERT(!eventName.empty());
	AKASSERT(gameObject);

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventName.unicode_str(), static_cast<AkGameObjectID>(gameObject->get_instance_id()),
														flags, eventCallback);

	if (playingID == AK_INVALID_PLAYING_ID)
	{
		ERROR_CHECK(AK_InvalidID, eventName);
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postEventID(const unsigned int eventID, const Object* gameObject)
{
	AKASSERT(gameObject);

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventID, static_cast<AkGameObjectID>(gameObject->get_instance_id()));

	if (playingID == AK_INVALID_PLAYING_ID) 
	{
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postEventIDCallback(const unsigned int eventID, const unsigned int flags, const Object* gameObject)
{
	AKASSERT(gameObject);

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventID, static_cast<AkGameObjectID>(gameObject->get_instance_id()), flags,
														eventCallback);

	if (playingID == AK_INVALID_PLAYING_ID)
	{
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}

bool Wwise::stopEvent(const int playingID, const int fadeTime, const int interpolation)
{
	AKASSERT(fadeTime >= 0);

	AK::SoundEngine::StopPlayingID(static_cast<AkPlayingID>(playingID), static_cast<AkTimeMs>(fadeTime), 
									static_cast<AkCurveInterpolation>(interpolation));

	return true;
}

bool Wwise::setSwitch(const String switchGroup, const String switchState, const Object* gameObject)
{
	AKASSERT(!switchGroup.empty());
	AKASSERT(!switchState.empty());
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::SetSwitch(switchGroup.unicode_str(), 
						switchState.unicode_str(), 
						static_cast<AkGameObjectID>(gameObject->get_instance_id())),
						"Switch " + switchGroup + " and state " + switchState);
}

bool Wwise::setSwitchID(const unsigned int switchGroupID, const unsigned int switchStateID, const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::SetSwitch(switchGroupID, switchStateID, static_cast<AkGameObjectID>(gameObject->get_instance_id())),
						"Switch ID " + String::num_int64(switchGroupID) + 
						" and switch state ID " + String::num_int64(switchStateID));
}

bool Wwise::setState(const String stateGroup, const String stateValue)
{
	AKASSERT(!stateGroup.empty());
	AKASSERT(!stateValue.empty());

	return ERROR_CHECK(AK::SoundEngine::SetState(stateGroup.unicode_str(), stateValue.unicode_str()),
						"Failed to set state " + stateGroup + " and value " + stateValue);
}

bool Wwise::setStateID(const unsigned int stateGroupID, const unsigned int stateValueID)
{
	return ERROR_CHECK(AK::SoundEngine::SetState(stateGroupID, stateValueID),
						"Failed to set state ID" + String::num_int64(stateGroupID) + " and value " + String::num_int64(stateValueID));
}

// todo: global rtpc
float Wwise::getRTPCValue(const String rtpcName, const Object* gameObject)
{
	AKASSERT(!rtpcName.empty());
	AKASSERT(gameObject);

	AkRtpcValue value;
	AK::SoundEngine::Query::RTPCValue_type type = AK::SoundEngine::Query::RTPCValue_GameObject;

	if (!ERROR_CHECK(AK::SoundEngine::Query::GetRTPCValue(rtpcName.unicode_str(), 
		static_cast<AkGameObjectID>(gameObject->get_instance_id()),
		static_cast<AkPlayingID>(0), value, type), rtpcName))
	{
		return INVALID_RTPC_VALUE;
	}

	return static_cast<float>(value);
}

float Wwise::getRTPCValueID(const unsigned int rtpcID, const Object* gameObject)
{
	AKASSERT(gameObject);

	AkRtpcValue value;
	AK::SoundEngine::Query::RTPCValue_type type = AK::SoundEngine::Query::RTPCValue_GameObject;

	if (!ERROR_CHECK(AK::SoundEngine::Query::GetRTPCValue(rtpcID, static_cast<AkGameObjectID>(gameObject->get_instance_id()),
		static_cast<AkPlayingID>(0), value, type), String::num_int64(rtpcID)))
	{
		return INVALID_RTPC_VALUE;
	}

	return static_cast<float>(value);
}

// todo: global rtpc
bool Wwise::setRTPCValue(const String rtpcName, const float rtpcValue, const Object* gameObject)
{
	AKASSERT(!rtpcName.empty());
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::SetRTPCValue(rtpcName.unicode_str(), static_cast<AkRtpcValue>(rtpcValue), 
						static_cast<AkGameObjectID>(gameObject->get_instance_id())), rtpcName);
}

bool Wwise::setRTPCValueID(const unsigned int rtpcID, const float rtpcValue, const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::SetRTPCValue(rtpcID, static_cast<AkRtpcValue>(rtpcValue), 
						static_cast<AkGameObjectID>(gameObject->get_instance_id())), String::num_int64(rtpcID));
}

bool Wwise::postTrigger(const String triggerName, const Object* gameObject)
{
	AKASSERT(!triggerName.empty());
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::PostTrigger(triggerName.unicode_str(),
						static_cast<AkGameObjectID>(gameObject->get_instance_id())), "Failed to post trigger " + triggerName);
}

bool Wwise::postTriggerID(const unsigned int triggerID, const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::PostTrigger(triggerID,
						static_cast<AkGameObjectID>(gameObject->get_instance_id())), "Failed to post trigger ID " + String::num_int64(triggerID));
}

unsigned int Wwise::postExternalSource(const String eventName, const Object* gameObject, const String sourceObjectName, const String fileName, const unsigned int idCodec)
{
	AKASSERT(!eventName.empty());
	AKASSERT(gameObject);
	AKASSERT(!sourceObjectName.empty());
	AKASSERT(!fileName.empty());

	AkExternalSourceInfo source;
	source.iExternalSrcCookie = AK::SoundEngine::GetIDFromString(sourceObjectName.unicode_str());

	AkOSChar* szFileOsString = nullptr;

	CONVERT_WIDE_TO_OSCHAR(fileName.unicode_str(), szFileOsString);

	source.szFile = szFileOsString;
	source.idCodec = idCodec;

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventName.unicode_str(), static_cast<AkGameObjectID>(gameObject->get_instance_id()), 0, NULL, 0, 1, &source);

	if (playingID == AK_INVALID_PLAYING_ID)
	{
		ERROR_CHECK(AK_InvalidID, eventName);
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postExternalSourceID(const unsigned int eventID, const Object* gameObject, const unsigned int sourceObjectID, const String fileName, const unsigned int idCodec)
{
	AKASSERT(gameObject);
	AKASSERT(!fileName.empty());

	AkExternalSourceInfo source;
	source.iExternalSrcCookie = sourceObjectID;

	AkOSChar* szFileOsString = nullptr;

	CONVERT_WIDE_TO_OSCHAR(fileName.unicode_str(), szFileOsString);

	source.szFile = szFileOsString;
	source.idCodec = idCodec;

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventID, static_cast<AkGameObjectID>(gameObject->get_instance_id()), 0, NULL, 0, 1, &source);

	if (playingID == AK_INVALID_PLAYING_ID)
	{
		ERROR_CHECK(AK_InvalidID, eventID);
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}
