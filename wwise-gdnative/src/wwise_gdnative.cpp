#include "wwise_gdnative.h"

#include <AK/Plugin/AllPluginsFactories.h>

#if defined(AK_REFLECT)
#include <AK/Plugin/AkReflectFXFactory.h>
#endif

#if defined(AK_MOTION)
#include <AK/Plugin/AkMotionSinkFactory.h>
#include <AK/Plugin/AkMotionSourceSourceFactory.h>
#endif

#if defined(AK_CONVOLUTION)
#include <AK/Plugin/AkConvolutionReverbFXFactory.h>
#endif

#if defined(AK_SOUNDSEED_GRAIN)
#include <AK/Plugin/AkSoundSeedGrainSourceFactory.h>
#endif

#if defined(AK_SOUNDSEED_AIR_IMPACT)
#include <AK/Plugin/AkSoundSeedWindSourceFactory.h>
#include <AK/Plugin/AkSoundSeedImpactFXFactory.h>
#include <AK/Plugin/AkSoundSeedWooshSourceFactory.h>
#endif

#if defined(RESONANCE_AUDIO)
#include <AK/Plugin/ResonanceAudioFXFactory.h>
#endif

using namespace godot;

CAkLock Wwise::signalDataLock;
std::unique_ptr<Array> Wwise::signalDataArray = nullptr;
std::unique_ptr<Array> Wwise::signalBankDataArray = nullptr;
int Wwise::signalCallbackDataMaxSize = 4096;

CAkLock g_localOutputLock;

#if defined(AK_ENABLE_ASSERTS)
void WwiseAssertHook(const char* in_pszExpression, const char* in_pszFileName, int in_lineNumber)
{
	String msg = "AKASSERT: " + String(in_pszExpression);
	Godot::print_warning(msg, "WwiseAssertHook", String(in_pszFileName), in_lineNumber);
	AKPLATFORM::OutputDebugMsg(msg.alloc_c_string());
}
#endif

void LocalOutput(AK::Monitor::ErrorCode in_eErrorCode, const AkOSChar* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID)
{
	AkAutoLock<CAkLock> autoLock(g_localOutputLock);

	if (in_eErrorCode != AK::Monitor::ErrorCode::ErrorCode_NoError)
	{
		Godot::print_error(String(in_pszError), "Wwise monitor", "", 0);
	}
}

Wwise::~Wwise() 
{
	shutdownWwiseSystems();

	signalDataArray = nullptr;
	signalBankDataArray = nullptr;

	Godot::print("Wwise has shut down");
}

#define REGISTER_GODOT_SIGNAL(callbackType) \
	register_signal<Wwise>(WwiseCallbackToSignal(callbackType), "data", GODOT_VARIANT_TYPE_DICTIONARY);

void Wwise::_register_methods()
{
	register_method("_process", &Wwise::_process);
	register_method("_notification", &Wwise::_notification);
	register_method("set_current_language", &Wwise::setCurrentLanguage);
	register_method("load_bank", &Wwise::loadBank);
	register_method("load_bank_id", &Wwise::loadBankID);
	register_method("load_bank_async", &Wwise::loadBankAsync);
	register_method("load_bank_async_id", &Wwise::loadBankAsyncID);
	register_method("unload_bank", &Wwise::unloadBank);
	register_method("unload_bank_id", &Wwise::unloadBankID);
	register_method("unload_bank_async", &Wwise::unloadBankAsync);
	register_method("unload_bank_async_id", &Wwise::unloadBankAsyncID);
	register_method("register_listener", &Wwise::registerListener);
	register_method("register_game_obj", &Wwise::registerGameObject);
	register_method("unregister_game_obj", &Wwise::unregisterGameObject);
	register_method("set_listeners", &Wwise::setListeners);
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
	register_method("get_source_play_position", &Wwise::getSourcePlayPosition);
	register_method("get_playing_segment_info", &Wwise::getPlayingSegmentInfo);
	register_method("set_game_obj_output_bus_volume", &Wwise::setGameObjectOutputBusVolume);
	register_method("set_game_obj_aux_send_values", &Wwise::setGameObjectAuxSendValues);
	register_method("set_obj_obstruction_and_occlusion", &Wwise::setObjectObstructionAndOcclusion); 
	register_method("set_geometry", &Wwise::setGeometry);
	register_method("remove_geometry", &Wwise::removeGeometry);
	register_method("register_spatial_listener", &Wwise::registerSpatialListener);
	register_method("set_room", &Wwise::setRoom);
	register_method("remove_room", &Wwise::removeRoom);
	register_method("set_portal", &Wwise::setPortal);
	register_method("remove_portal", &Wwise::removePortal);
	register_method("set_game_obj_in_room", &Wwise::setGameObjectInRoom);
	register_method("remove_game_obj_from_room", &Wwise::removeGameObjectFromRoom);
	register_method("set_early_reflections_aux_send", &Wwise::setEarlyReflectionsAuxSend);
	register_method("set_early_reflections_volume", &Wwise::setEarlyReflectionsVolume);
	register_method("add_output", &Wwise::addOutput);
	register_method("remove_output", &Wwise::removeOutput);
	register_method("suspend", &Wwise::suspend);
	register_method("wakeup_from_suspend", &Wwise::wakeupFromSuspend);
	register_method("update_audio_room", &Wwise::updateAudioRoom);
	register_method("is_listener_inside_room", &Wwise::isListenerInsideRoom);

	REGISTER_GODOT_SIGNAL(AK_EndOfEvent);
	REGISTER_GODOT_SIGNAL(AK_EndOfDynamicSequenceItem);
	REGISTER_GODOT_SIGNAL(AK_Marker);
	REGISTER_GODOT_SIGNAL(AK_Duration);
	REGISTER_GODOT_SIGNAL(AK_SpeakerVolumeMatrix);
	REGISTER_GODOT_SIGNAL(AK_Starvation);
	REGISTER_GODOT_SIGNAL(AK_MusicPlaylistSelect);
	REGISTER_GODOT_SIGNAL(AK_MusicPlayStarted);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncBeat);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncBar);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncEntry);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncExit);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncGrid);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncUserCue);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncPoint);
	REGISTER_GODOT_SIGNAL(AK_MusicSyncAll);
	REGISTER_GODOT_SIGNAL(AK_MIDIEvent);
	REGISTER_GODOT_SIGNAL(AK_CallbackBits);
	REGISTER_GODOT_SIGNAL(AK_EnableGetSourcePlayPosition);
	REGISTER_GODOT_SIGNAL(AK_EnableGetMusicPlayPosition);
	REGISTER_GODOT_SIGNAL(AK_EnableGetSourceStreamBuffering);
	register_signal<Wwise>("bank_callback", "data", GODOT_VARIANT_TYPE_DICTIONARY);
}

void Wwise::_init()
{
	signalDataArray = std::make_unique<Array>();
	signalBankDataArray = std::make_unique<Array>();

	projectSettings = ProjectSettings::get_singleton();
	AKASSERT(projectSettings);

	bool initialisationResult = initialiseWwiseSystems();

	if (!initialisationResult)
	{
		ERROR_CHECK(AK_Fail, "Wwise systems initialisation failed!");
		return;
	}
	else
	{
		Godot::print("Wwise systems initialisation succeeded");
	}

	String basePath = getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "base_path");
	String banksPath = basePath;

#ifdef AK_WIN
	String platformBanksSuffix = "/Windows";
#elif defined(AK_MAC_OS_X)
	String platformBanksSuffix = "/Mac";
#elif defined(AK_IOS)
	String platformBanksSuffix = "/iOS";
#elif defined(AK_ANDROID)
	String platformBanksSuffix = "/Android";
#elif defined(AK_LINUX)
	String platformBanksSuffix = "/Linux";
#else
#error "Platform not supported"
#endif

	if (OS::get_singleton()->has_feature("standalone"))
	{
		MAP_PATH_STANDALONE(basePath);

#if defined(AK_WIN) || defined(AK_MAC_OS_X) || defined(AK_LINUX)
		bool copyBanksResult = CopyDirectory(banksPath + platformBanksSuffix, OS::get_singleton()->get_user_data_dir() + "/wwise/GeneratedSoundBanks" + platformBanksSuffix);

		if (!copyBanksResult)
		{
			ERROR_CHECK(AK_Fail, "Copying banks to user:// failed!");
		}
#endif
	}
	else
	{
		MAP_PATH(basePath);
	}

	basePath += platformBanksSuffix;

	bool setBasePathResult = setBasePath(basePath);
	AKASSERT(setBasePathResult);

	String startupLanguage = getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "startup_language");

	bool setCurrentLanguageResult = setCurrentLanguage(startupLanguage);
	AKASSERT(setCurrentLanguageResult);

	signalCallbackDataMaxSize = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "callback_manager_buffer_size"));

#if !defined(AK_OPTIMIZED)
	const bool engineLogging =
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "engine_logging"));

	if (engineLogging)
	{
		ERROR_CHECK(AK::Monitor::SetLocalOutput(AK::Monitor::ErrorLevel_All, static_cast<AK::Monitor::LocalOutputFunc>(LocalOutput)), 
					"Failed to set ErrorLevel_All");
	}
#endif
}

void Wwise::_process(const float delta)
{
	emitSignals();
	emitBankSignals();
	ERROR_CHECK(AK::SoundEngine::RenderAudio(), "");
}

void Wwise::_notification(int notification)
{
#if defined(AK_ANDROID) || defined(AK_IOS)
	if (notification == NOTIFICATION_WM_FOCUS_OUT)
	{
		Wwise::suspend(false);
	}
	if (notification == NOTIFICATION_WM_FOCUS_IN)
	{
		Wwise::wakeupFromSuspend();
	}
#endif 
}

bool Wwise::setBasePath(const String basePath)
{
	AKASSERT(!basePath.empty());

	AkOSChar* basePathOsString = nullptr;

	const char* basePathChar = basePath.alloc_c_string();
	CONVERT_CHAR_TO_OSCHAR(basePathChar, basePathOsString);
	AKASSERT(basePathOsString);

	return ERROR_CHECK(lowLevelIO.SetBasePath(basePathOsString), basePath);
}

bool Wwise::setCurrentLanguage(const String language)
{
	AKASSERT(!language.empty());

	AkOSChar* languageOsString = nullptr;

	const char* languageChar = language.alloc_c_string();
	CONVERT_CHAR_TO_OSCHAR(languageChar, languageOsString);
	AKASSERT(languageOsString);

	return ERROR_CHECK(AK::StreamMgr::SetCurrentLanguage(languageOsString), language);
}

bool Wwise::loadBank(const String bankName)
{
	AkBankID bankID;
	AKASSERT(!bankName.empty());

	return ERROR_CHECK(AK::SoundEngine::LoadBank(bankName.alloc_c_string(), bankID), bankName);
}

bool Wwise::loadBankID(const unsigned int bankID)
{
	return ERROR_CHECK(AK::SoundEngine::LoadBank(bankID), "ID " + String::num_int64(bankID));
}

bool Wwise::loadBankAsync(const String bankName)
{
	AkBankID bankID = 0;
	AKASSERT(!bankName.empty());

	return ERROR_CHECK(AK::SoundEngine::LoadBank(bankName.alloc_c_string(), (AkBankCallbackFunc)bankCallback, nullptr, bankID), "ID " + String::num_int64(bankID));
}

bool Wwise::loadBankAsyncID(const unsigned int bankID)
{
	return ERROR_CHECK(AK::SoundEngine::LoadBank(bankID, (AkBankCallbackFunc)bankCallback, nullptr), "ID " + String::num_int64(bankID));
}

bool Wwise::unloadBank(const String bankName)
{
	AKASSERT(!bankName.empty());

	return ERROR_CHECK(AK::SoundEngine::UnloadBank(bankName.alloc_c_string(), NULL), bankName);
}

bool Wwise::unloadBankID(const unsigned int bankID)
{
	return ERROR_CHECK(AK::SoundEngine::UnloadBank(bankID, NULL), "ID " + String::num_int64(bankID) + " failed");
}

bool Wwise::unloadBankAsync(const String bankName)
{
	AKASSERT(!bankName.empty());

	return ERROR_CHECK(AK::SoundEngine::UnloadBank(bankName.alloc_c_string(), NULL, (AkBankCallbackFunc)bankCallback, nullptr), "Loading bank: " + bankName + " failed");
}

bool Wwise::unloadBankAsyncID(const unsigned int bankID)
{
	return ERROR_CHECK(AK::SoundEngine::UnloadBank(bankID, NULL, (AkBankCallbackFunc)bankCallback, nullptr), "ID " + String::num_int64(bankID) + " failed");
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

bool Wwise::unregisterGameObject(const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::UnregisterGameObj(static_cast<AkGameObjectID>(gameObject->get_instance_id())), "Failed to unregister Game Object: " + String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::setListeners(const Object* emitter, const Object* listener)
{
	static const int kNumLstnrsForEm = 1;
	static const AkGameObjectID aLstnrsForEmitter[kNumLstnrsForEm] = { static_cast<AkGameObjectID>(listener->get_instance_id()) };
	return ERROR_CHECK(AK::SoundEngine::SetListeners(static_cast<AkGameObjectID>(emitter->get_instance_id()), aLstnrsForEmitter, 1), "Failed to associate Emitter with listener");
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

	Vector3 position = Vector3(-origin.x * 0.1f, -origin.y * 0.1f, zDepth);
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

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventName.alloc_c_string(), static_cast<AkGameObjectID>(gameObject->get_instance_id()));

	if (playingID == AK_INVALID_PLAYING_ID) 
	{
		ERROR_CHECK(AK_InvalidID, eventName);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postEventCallback(const String eventName, const unsigned int flags, const Object* gameObject)
{
	AKASSERT(!eventName.empty());
	AKASSERT(gameObject);

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventName.alloc_c_string(), static_cast<AkGameObjectID>(gameObject->get_instance_id()),
														flags, eventCallback);

	if (playingID == AK_INVALID_PLAYING_ID)
	{
		ERROR_CHECK(AK_InvalidID, eventName);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postEventID(const unsigned int eventID, const Object* gameObject)
{
	AKASSERT(gameObject);

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventID, static_cast<AkGameObjectID>(gameObject->get_instance_id()));

	if (playingID == AK_INVALID_PLAYING_ID) 
	{
		ERROR_CHECK(AK_InvalidID, eventID);
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
		ERROR_CHECK(AK_InvalidID, eventID);
	}

	return static_cast<unsigned int>(playingID);
}

bool Wwise::stopEvent(const int playingID, const int fadeTime, const int interpolation)
{
	AKASSERT(fadeTime >= 0);

	AK::SoundEngine::ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType::AkActionOnEventType_Stop, static_cast<AkPlayingID>(playingID), static_cast<AkTimeMs>(fadeTime),
					static_cast<AkCurveInterpolation>(interpolation));

	return true;
}

bool Wwise::setSwitch(const String switchGroup, const String switchState, const Object* gameObject)
{
	AKASSERT(!switchGroup.empty());
	AKASSERT(!switchState.empty());
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::SetSwitch(switchGroup.alloc_c_string(), 
						switchState.alloc_c_string(), 
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

	return ERROR_CHECK(AK::SoundEngine::SetState(stateGroup.alloc_c_string(), stateValue.alloc_c_string()),
						"Failed to set state " + stateGroup + " and value " + stateValue);
}

bool Wwise::setStateID(const unsigned int stateGroupID, const unsigned int stateValueID)
{
	return ERROR_CHECK(AK::SoundEngine::SetState(stateGroupID, stateValueID),
						"Failed to set state ID" + String::num_int64(stateGroupID) + " and value " + String::num_int64(stateValueID));
}

float Wwise::getRTPCValue(const String rtpcName, const Object* gameObject)
{
	AKASSERT(!rtpcName.empty());
	AkRtpcValue value;
	AK::SoundEngine::Query::RTPCValue_type type = AK::SoundEngine::Query::RTPCValue_Default;
	AkGameObjectID gameObjectID = AK_INVALID_GAME_OBJECT;

	if (gameObject)
	{
		type = AK::SoundEngine::Query::RTPCValue_GameObject;
		gameObjectID = static_cast<AkGameObjectID>(gameObject->get_instance_id());
	}
	else
	{
		type = AK::SoundEngine::Query::RTPCValue_Global;
		gameObjectID = AK_INVALID_GAME_OBJECT;
	}

	if (!ERROR_CHECK(AK::SoundEngine::Query::GetRTPCValue(rtpcName.alloc_c_string(), 
		gameObjectID,
		static_cast<AkPlayingID>(0), value, type), rtpcName))
	{
		return INVALID_RTPC_VALUE;
	}

	return static_cast<float>(value);
}

float Wwise::getRTPCValueID(const unsigned int rtpcID, const Object* gameObject)
{
	AkRtpcValue value;
	AK::SoundEngine::Query::RTPCValue_type type = AK::SoundEngine::Query::RTPCValue_Default;
	AkGameObjectID gameObjectID = AK_INVALID_GAME_OBJECT;

	if (gameObject)
	{
		type = AK::SoundEngine::Query::RTPCValue_GameObject;
		gameObjectID = static_cast<AkGameObjectID>(gameObject->get_instance_id());
	}
	else
	{
		type = AK::SoundEngine::Query::RTPCValue_Global;
		gameObjectID = AK_INVALID_GAME_OBJECT;
	}

	if (!ERROR_CHECK(AK::SoundEngine::Query::GetRTPCValue(rtpcID, gameObjectID,
		static_cast<AkPlayingID>(0), value, type), String::num_int64(rtpcID)))
	{
		return INVALID_RTPC_VALUE;
	}

	return static_cast<float>(value);
}

bool Wwise::setRTPCValue(const String rtpcName, const float rtpcValue, const Object* gameObject)
{
	AKASSERT(!rtpcName.empty());
	AkGameObjectID gameObjectID = AK_INVALID_GAME_OBJECT;

	if (gameObject)
	{
		gameObjectID = static_cast<AkGameObjectID>(gameObject->get_instance_id());
	}
	else
	{
		gameObjectID = AK_INVALID_GAME_OBJECT;
	}

	return ERROR_CHECK(AK::SoundEngine::SetRTPCValue(rtpcName.alloc_c_string(), static_cast<AkRtpcValue>(rtpcValue),
		gameObjectID), rtpcName);
}

bool Wwise::setRTPCValueID(const unsigned int rtpcID, const float rtpcValue, const Object* gameObject)
{
	AkGameObjectID gameObjectID = AK_INVALID_GAME_OBJECT;

	if (gameObject)
	{
		gameObjectID = static_cast<AkGameObjectID>(gameObject->get_instance_id());
	}
	else
	{
		gameObjectID = AK_INVALID_GAME_OBJECT;
	}

	return ERROR_CHECK(AK::SoundEngine::SetRTPCValue(rtpcID, static_cast<AkRtpcValue>(rtpcValue),
		gameObjectID), String::num_int64(rtpcID));
}

bool Wwise::postTrigger(const String triggerName, const Object* gameObject)
{
	AKASSERT(!triggerName.empty());
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::PostTrigger(triggerName.alloc_c_string(),
						static_cast<AkGameObjectID>(gameObject->get_instance_id())), "Failed to post trigger " + triggerName);
}

bool Wwise::postTriggerID(const unsigned int triggerID, const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SoundEngine::PostTrigger(triggerID,
						static_cast<AkGameObjectID>(gameObject->get_instance_id())), "Failed to post trigger ID " + String::num_int64(triggerID));
}

unsigned int Wwise::postExternalSource(const String eventName, const Object* gameObject, const String sourceObjectName, const String fileName, 
	const unsigned int idCodec)
{
	AKASSERT(!eventName.empty());
	AKASSERT(gameObject);
	AKASSERT(!sourceObjectName.empty());
	AKASSERT(!fileName.empty());

	AkExternalSourceInfo source;
	source.iExternalSrcCookie = AK::SoundEngine::GetIDFromString(sourceObjectName.alloc_c_string());

	AkOSChar* szFileOsString = nullptr;

	CONVERT_CHAR_TO_OSCHAR(fileName.alloc_c_string(), szFileOsString);

	source.szFile = szFileOsString;
	source.idCodec = idCodec;

	AkPlayingID playingID = AK::SoundEngine::PostEvent(eventName.alloc_c_string(), static_cast<AkGameObjectID>(gameObject->get_instance_id()), 0, nullptr, 0, 1, &source);

	if (playingID == AK_INVALID_PLAYING_ID)
	{
		ERROR_CHECK(AK_InvalidID, eventName);
		return static_cast<unsigned int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<unsigned int>(playingID);
}

unsigned int Wwise::postExternalSourceID(const unsigned int eventID, const Object* gameObject, const unsigned int sourceObjectID, 
	const String fileName, const unsigned int idCodec)
{
	AKASSERT(gameObject);
	AKASSERT(!fileName.empty());

	AkExternalSourceInfo source;
	source.iExternalSrcCookie = sourceObjectID;

	AkOSChar* szFileOsString = nullptr;

	CONVERT_CHAR_TO_OSCHAR(fileName.alloc_c_string(), szFileOsString);

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

int Wwise::getSourcePlayPosition(const unsigned int playingID, const bool extrapolate)
{
	AkTimeMs position;
	AKRESULT result = AK::SoundEngine::GetSourcePlayPosition(static_cast<AkPlayingID>(playingID), &position, extrapolate);

	if (result == AK_Fail)
	{
		ERROR_CHECK(result, "Failed to get Source Play Position for playing ID " + String::num_int64(playingID));
		return static_cast<int>(AK_INVALID_PLAYING_ID);
	}

	return static_cast<int>(position);
}

Dictionary Wwise::getPlayingSegmentInfo(const unsigned int playingID, const bool extrapolate)
{
	AkSegmentInfo segmentInfo;
	AKRESULT result = AK::MusicEngine::GetPlayingSegmentInfo(static_cast<AkPlayingID>(playingID), segmentInfo, extrapolate);

	if (result == AK_Fail)
	{
		ERROR_CHECK(result, "Failed to get playing Segment Info for playing ID " + String::num_int64(playingID));
	}

	Dictionary segment;
	segment["fBarDuration"] = static_cast<float>(segmentInfo.fBarDuration);
	segment["fBeatDuration"] = static_cast<float>(segmentInfo.fBeatDuration);
	segment["fGridDuration"] = static_cast<float>(segmentInfo.fGridDuration);
	segment["fGridOffset"] = static_cast<float>(segmentInfo.fGridOffset);
	segment["iActiveDuration"] = static_cast<int>(segmentInfo.iActiveDuration);
	segment["iCurrentPosition"] = static_cast<int>(segmentInfo.iCurrentPosition);
	segment["iPostExitDuration"] = static_cast<int>(segmentInfo.iPostExitDuration);
	segment["iPreEntryDuration"] = static_cast<int>(segmentInfo.iPreEntryDuration);
	segment["iRemainingLookAheadTime"] = static_cast<int>(segmentInfo.iRemainingLookAheadTime);

	return segment;
}

bool Wwise::setGameObjectOutputBusVolume(const unsigned int gameObjectID, const unsigned int listenerID, float fControlValue)
{
	return ERROR_CHECK(AK::SoundEngine::SetGameObjectOutputBusVolume(static_cast<AkGameObjectID>(gameObjectID), 
																		static_cast<AkGameObjectID>(listenerID), fControlValue), "Could not set the Game Object Outpus Bus Volume");
}

bool Wwise::setGameObjectAuxSendValues(const unsigned int gameObjectID, const Array akAuxSendValues, const unsigned int sendValues)
{
	AkAuxSendValue environments[AK_MAX_ENVIRONMENTS];

	for (int i = 0; i < akAuxSendValues.size(); i++)
	{
		Dictionary auxBusData = akAuxSendValues[i];
		environments[i].auxBusID = static_cast<const unsigned int>(auxBusData["aux_bus_id"]);
		environments[i].fControlValue = static_cast<float>(auxBusData["control_value"]);
		environments[i].listenerID = AK_INVALID_GAME_OBJECT;
	}

	return ERROR_CHECK(AK::SoundEngine::SetGameObjectAuxSendValues(static_cast<AkGameObjectID>(gameObjectID),
																	environments, sendValues), "Could not set the Game Object Aux Send Values");
}

bool Wwise::setObjectObstructionAndOcclusion(const unsigned int gameObjectID, const unsigned int listenerID, float fCalculatedObs, float fCalculatedOcc)
{
	return ERROR_CHECK(AK::SoundEngine::SetObjectObstructionAndOcclusion(static_cast<AkGameObjectID>(gameObjectID), 
																			static_cast<AkGameObjectID>(listenerID), fCalculatedObs, fCalculatedOcc), "Could not set Obstruction and Occlusion");
}

bool Wwise::setGeometry(const Array vertices, const Array triangles, const Resource* acousticTexture, const float occlusionValue, 
						const Object* gameObject, bool enableDiffraction, bool enableDiffractionOnBoundaryEdges, const Object* associatedRoom)
{
	AKASSERT(!vertices.empty());
	AKASSERT(!triangles.empty());
	AKASSERT(gameObject);

	AkGeometryParams geometry;

	int vertexCount = vertices.size();
	auto vertRemap = std::make_unique<int[]>(vertexCount);
	PoolVector3Array uniqueVerts;
	Dictionary vertDict;

	// removing duplicate verts
	for (int v = 0; v < vertexCount; ++v)
	{
		int vertIdx = 0;
		
		if (!FindMatchingVertex(vertices[v], vertDict, vertIdx))
		{
			vertIdx = uniqueVerts.size();
			uniqueVerts.append(vertices[v]);
			vertDict[vertices[v]] = vertIdx;
		}
		vertRemap[v] = vertIdx;
	}

	vertexCount = uniqueVerts.size();
	auto akVertices = std::make_unique<AkVertex[]>(vertexCount);

	for (int i = 0; i < vertexCount; i++)
	{
		Vector3 point = uniqueVerts[i];
		AkVertex v;
		v.X = -point.x;		// Seems to be flipped in Wwise otherwise
		v.Y = point.y;
		v.Z = point.z;

		akVertices[i] = v;
	}

	int numTriangles = triangles.size() / 3;

	if ((numTriangles % 3) != 0)
	{
		Godot::print("Wrong number of triangles on mesh {0}", String::num_int64(gameObject->get_instance_id()));
	}

	auto akTriangles = std::make_unique<AkTriangle[]>(numTriangles);


	if (acousticTexture)
	{
		AkAcousticSurface akSurfaces[1];

		geometry.NumSurfaces = 1;

		AkAcousticTexture akAcousticTexture;
		String acousticTextureName = acousticTexture->get("name");
		akAcousticTexture.ID = AK::SoundEngine::GetIDFromString(acousticTextureName.alloc_c_string());

		// Not possible to get the acoustic texture values through AK::SoundEngine, maybe looking at WAAPI
		akAcousticTexture.fAbsorptionHigh = static_cast<float>(acousticTexture->get("absorption_high"));
		akAcousticTexture.fAbsorptionLow = static_cast<float>(acousticTexture->get("absorption_low"));
		akAcousticTexture.fAbsorptionMidHigh = static_cast<float>(acousticTexture->get("absorption_mid_high"));
		akAcousticTexture.fAbsorptionMidLow = static_cast<float>(acousticTexture->get("absorption_mid_low"));
		akAcousticTexture.fAbsorptionOffset = static_cast<float>(acousticTexture->get("absorption_offset"));
		akAcousticTexture.fScattering = static_cast<float>(acousticTexture->get("scattering"));

		akSurfaces[0].textureID = akAcousticTexture.ID;
		akSurfaces[0].occlusion = occlusionValue;
		akSurfaces[0].strName = acousticTextureName.alloc_c_string();

		geometry.Surfaces = akSurfaces;
	}

	int triangleIdx = 0;

	for (int i = 0; i < numTriangles; i++)
	{
		AkTriangle t;
		t.point0 = vertRemap[static_cast<unsigned int>(triangles[3 * i + 0])];
		t.point1 = vertRemap[static_cast<unsigned int>(triangles[3 * i + 1])];
		t.point2 = vertRemap[static_cast<unsigned int>(triangles[3 * i + 2])];
		t.surface = acousticTexture ? 0 : AK_INVALID_SURFACE;

		akTriangles[triangleIdx] = t;

		if (t.point0 != t.point1 && t.point0 != t.point2 && t.point1 != t.point2)
		{
			++triangleIdx;
		}
		else
		{
			Godot::print("Skipped degenerate triangles on mesh {0}", String::num_int64(gameObject->get_instance_id()));
		}
	}

	geometry.Vertices = akVertices.get();
	geometry.NumVertices = vertexCount;
	geometry.Triangles = akTriangles.get();
	geometry.NumTriangles = triangleIdx;

	geometry.EnableDiffraction = enableDiffraction;
	geometry.EnableDiffractionOnBoundaryEdges = enableDiffractionOnBoundaryEdges;
	geometry.RoomID = associatedRoom ? static_cast<AkRoomID>(associatedRoom->get_instance_id()) : AkRoomID();

	return ERROR_CHECK(AK::SpatialAudio::SetGeometry(static_cast<AkGeometrySetID>(gameObject->get_instance_id()), geometry), "Failed to register geometry");
}

bool Wwise::removeGeometry(const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SpatialAudio::RemoveGeometry(static_cast<AkGeometrySetID>(gameObject->get_instance_id())), "Failed to remove geometry");
}

bool Wwise::registerSpatialListener(const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SpatialAudio::RegisterListener(static_cast<AkGameObjectID>(gameObject->get_instance_id())), "Failed to register Spatial Audio Listener");
}

bool Wwise::setRoom(const Object* gameObject, const unsigned int akAuxBusID, const String gameObjectName)
{
	AKASSERT(gameObject);

	AkRoomParams roomParams;
	roomParams.ReverbAuxBus = akAuxBusID;
	roomParams.strName = gameObjectName.alloc_c_string();
	return ERROR_CHECK(AK::SpatialAudio::SetRoom(static_cast<AkRoomID>(gameObject->get_instance_id()), roomParams), "Failed to set Room for Game Object: " 
																													+ String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::removeRoom(const Object* gameObject)
{
	return ERROR_CHECK(AK::SpatialAudio::RemoveRoom(static_cast<AkRoomID>(gameObject->get_instance_id())), "Failed to remove Room for Game Object: " 
																											+ String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::setPortal(const Object* gameObject, const Transform transform, const Vector3 extent, const Object* frontRoom, const Object* backRoom, bool enabled, const String portalName)
{
	AKASSERT(gameObject);

	AkVector position;	GetAkVector(transform, position, VectorType::POSITION);
	AkVector forward;	GetAkVector(transform, forward, VectorType::FORWARD);
	AkVector up;		GetAkVector(transform, up, VectorType::UP);

	AkTransform akTransform;
	akTransform.Set(position, forward, up);

	AkVector akExtent;	Vector3ToAkVector(extent, akExtent);

	AkRoomID roomID;

	AkPortalParams portalParams;
	portalParams.Transform = akTransform;
	portalParams.Extent = akExtent;
	portalParams.FrontRoom = frontRoom ? static_cast<AkRoomID>(frontRoom->get_instance_id()) : static_cast<AkRoomID>(INVALID_ROOM_ID);
	portalParams.BackRoom = backRoom ? static_cast<AkRoomID>(backRoom->get_instance_id()) : static_cast<AkRoomID>(INVALID_ROOM_ID);
	portalParams.bEnabled = enabled;
	portalParams.strName = portalName.alloc_c_string();

	return ERROR_CHECK(AK::SpatialAudio::SetPortal(static_cast<AkPortalID>(gameObject->get_instance_id()), portalParams), "Failed to set Portal on GameObject: " 
																														  + String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::removePortal(const Object* gameObject)
{
	AKASSERT(gameObject);
	return ERROR_CHECK(AK::SpatialAudio::RemovePortal(static_cast<AkPortalID>(gameObject->get_instance_id())), "Failed to remove Portal for Game Object: " 
																												+ String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::setGameObjectInRoom(const Object* gameObject, const Object* room)
{
	AKASSERT(gameObject);
	AKASSERT(room);
	return ERROR_CHECK(AK::SpatialAudio::SetGameObjectInRoom(static_cast<AkGameObjectID>(gameObject->get_instance_id()), static_cast<AkRoomID>(room->get_instance_id())), 
																						"Failed to set Game Object in Room: " + String::num_int64(room->get_instance_id()));
}

bool Wwise::removeGameObjectFromRoom(const Object* gameObject)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SpatialAudio::SetGameObjectInRoom(static_cast<AkGameObjectID>(gameObject->get_instance_id()), INVALID_ROOM_ID), "Failed to remove Game Object from Room: "
																																			+ String::num_int64(gameObject->get_instance_id()));
}

bool Wwise::setEarlyReflectionsAuxSend(const Object* gameObject, const unsigned int auxBusID)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SpatialAudio::SetEarlyReflectionsAuxSend(static_cast<AkGameObjectID>(gameObject->get_instance_id()), static_cast<AkAuxBusID>(auxBusID)), "Failed to set Early Reflections Aux Send");
}

bool Wwise::setEarlyReflectionsVolume(const Object* gameObject, const float volume)
{
	AKASSERT(gameObject);

	return ERROR_CHECK(AK::SpatialAudio::SetEarlyReflectionsVolume(static_cast<AkGameObjectID>(gameObject->get_instance_id()), volume), "Failed to set Early Reflections volume");
}

bool Wwise::addOutput(const String shareSet, const unsigned int outputID)
{
	AkOutputSettings outputSettings(shareSet.alloc_c_string(), outputID);

	return ERROR_CHECK(AK::SoundEngine::AddOutput(outputSettings), "Failed to add share set to output ID: " + String::num_int64(outputID));
}

bool Wwise::removeOutput(const unsigned int outputID)
{
	return ERROR_CHECK(AK::SoundEngine::RemoveOutput(outputID), "Failed to remove output ID: " + String::num_int64(outputID));
}

bool Wwise::suspend(bool renderAnyway)
{
	return ERROR_CHECK(AK::SoundEngine::Suspend(renderAnyway), "Failed to suspend SoundEngine");
}

bool Wwise::wakeupFromSuspend()
{
	return ERROR_CHECK(AK::SoundEngine::WakeupFromSuspend(), "Failed to wake up SoundEngine from suspend");
}

void Wwise::updateAudioRoom(const Spatial *room, bool enabled)
{
	if (enabled)
	{
		if (!enabledRooms.has(room))
		{
			enabledRooms.append(room);
		}
	}
	else
	{
		int idx = enabledRooms.find(room);
		if (idx != -1)
		{
			enabledRooms.remove(idx);
		}
	}

	if (enabledRooms.size() > 0)
	{
		Spatial* currentRoom = enabledRooms[enabledRooms.size() - 1];
		RoomProperties roomProperties;	getRoomProperties(currentRoom, roomProperties);
		AkUniqueID roomEffectsBusId = static_cast<AkUniqueID>(currentRoom->get("room_effects_bus"));
		ERROR_CHECK(AK::SoundEngine::SendPluginCustomGameData(roomEffectsBusId, AK_INVALID_GAME_OBJECT, AkPluginType::AkPluginTypeMixer,
												  resonanceCompanyId, roomEffectsPluginId, &roomProperties, sizeof(RoomProperties)), "Sending Resonance Audio plugin data to Wwise failed.");
	}
	else
	{
		AkUniqueID roomEffectsBusId = static_cast<AkUniqueID>(room->get("room_effects_bus"));
		ERROR_CHECK(AK::SoundEngine::SendPluginCustomGameData(roomEffectsBusId, AK_INVALID_GAME_OBJECT, AkPluginType::AkPluginTypeMixer,
												  resonanceCompanyId, roomEffectsPluginId, nullptr, 0U), "Sending Resonance Audio plugin data to Wwise failed.");
	}
}

bool Wwise::isListenerInsideRoom(const Spatial* room, const Spatial* listener)
{
	if (!listener)
	{
		return false;
	}

	Vector3 relativePosition = listener->get_global_transform().origin - room->get_global_transform().origin;
	Quat rotationInverse = Quat(room->get_global_transform().basis.orthonormalized()).inverse();
	scaleVector(room->get_scale(), static_cast<Vector3>(room->get("size")), bounds.size);		

	return hasPoint(bounds, relativePosition, rotationInverse);
}

void Wwise::eventCallback(AkCallbackType callbackType, AkCallbackInfo* callbackInfo)
{
	AkAutoLock<CAkLock> ScopedLock(signalDataLock);

	Dictionary signalData;
	signalData["callbackType"] = static_cast<unsigned int>(callbackType);

	switch (callbackType)
	{
	case AK_EndOfEvent:
	{
		AkEventCallbackInfo* eventInfo = static_cast<AkEventCallbackInfo*>(callbackInfo);
		signalData["eventID"] = static_cast<unsigned int>(eventInfo->eventID);
		signalData["gameObjID"] = static_cast<unsigned int>(eventInfo->gameObjID);
		signalData["playingID"] = static_cast<unsigned int>(eventInfo->playingID);
		break;
	}
	case AK_EndOfDynamicSequenceItem:
	{
		AkDynamicSequenceItemCallbackInfo* dynamicSequenceItemInfo = static_cast<AkDynamicSequenceItemCallbackInfo*>(callbackInfo);
		signalData["audioNodeID"] = static_cast<unsigned int>(dynamicSequenceItemInfo->audioNodeID);
		signalData["gameObjID"] = static_cast<unsigned int>(dynamicSequenceItemInfo->gameObjID);
		signalData["playingID"] = static_cast<unsigned int>(dynamicSequenceItemInfo->playingID);
		break;
	}
	case AK_Marker:
	{
		AkMarkerCallbackInfo* markerInfo = static_cast<AkMarkerCallbackInfo*>(callbackInfo);
		signalData["uIdentifier"] = static_cast<unsigned int>(markerInfo->uIdentifier);
		signalData["uPosition"] = static_cast<unsigned int>(markerInfo->uPosition);
		signalData["strLabel"] = String(markerInfo->strLabel);
		break;
	}
	case AK_Duration:
	{
		AkDurationCallbackInfo* durationInfo = static_cast<AkDurationCallbackInfo*>(callbackInfo);
		signalData["audioNodeID"] = static_cast<unsigned int>(durationInfo->audioNodeID);
		signalData["bStreaming"] = durationInfo->bStreaming;
		signalData["eventID"] = static_cast<unsigned int>(durationInfo->eventID);
		signalData["fDuration"] = static_cast<float>(durationInfo->fDuration);
		signalData["fEstimatedDuration"] = static_cast<float>(durationInfo->fEstimatedDuration);
		signalData["gameObjID"] = static_cast<unsigned int>(durationInfo->gameObjID);
		signalData["mediaID"] = static_cast<unsigned int>(durationInfo->mediaID);
		signalData["playingID"] = static_cast<unsigned int>(durationInfo->playingID);
		break;
	}
	case AK_SpeakerVolumeMatrix:
	{
		AkSpeakerVolumeMatrixCallbackInfo* speakerVolumeMatrixInfo = static_cast<AkSpeakerVolumeMatrixCallbackInfo*>(callbackInfo);
		signalData["eventID"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->eventID);
		signalData["gameObjID"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->gameObjID);

		Dictionary inputConfig;
		inputConfig["uNumChannels"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->inputConfig.uNumChannels);
		inputConfig["eConfigType"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->inputConfig.eConfigType);
		inputConfig["uChannelMask"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->inputConfig.uChannelMask);
		signalData["inputConfig"] = inputConfig;

		Dictionary outputConfig;
		outputConfig["uNumChannels"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->outputConfig.uNumChannels);
		outputConfig["eConfigType"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->outputConfig.eConfigType);
		outputConfig["uChannelMask"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->outputConfig.uChannelMask);
		signalData["outputConfig"] = outputConfig;

		signalData["playingID"] = static_cast<unsigned int>(speakerVolumeMatrixInfo->playingID);
		break;
	}
	case AK_Starvation:
	{
		AkEventCallbackInfo* eventInfo = static_cast<AkEventCallbackInfo*>(callbackInfo);
		signalData["eventID"] = static_cast<unsigned int>(eventInfo->eventID);
		signalData["gameObjID"] = static_cast<unsigned int>(eventInfo->gameObjID);
		signalData["playingID"] = static_cast<unsigned int>(eventInfo->playingID);
		break;
	}
	case AK_MusicPlaylistSelect:
	{
		AkMusicPlaylistCallbackInfo* musicPlaylistInfo = static_cast<AkMusicPlaylistCallbackInfo*>(callbackInfo);
		signalData["eventID"] = static_cast<unsigned int>(musicPlaylistInfo->eventID);
		signalData["gameObjID"] = static_cast<unsigned int>(musicPlaylistInfo->gameObjID);
		signalData["playingID"] = static_cast<unsigned int>(musicPlaylistInfo->playingID);
		signalData["playlistID"] = static_cast<unsigned int>(musicPlaylistInfo->playlistID);
		signalData["uNumPlaylistItems"] = static_cast<unsigned int>(musicPlaylistInfo->uNumPlaylistItems);
		signalData["uPlaylistItemDone"] = static_cast<unsigned int>(musicPlaylistInfo->uPlaylistItemDone);
		signalData["uPlaylistSelection"] = static_cast<unsigned int>(musicPlaylistInfo->uPlaylistSelection);
		break;
	}
	case AK_MusicPlayStarted:
	{
		AkEventCallbackInfo* eventInfo = static_cast<AkEventCallbackInfo*>(callbackInfo);
		signalData["eventID"] = static_cast<unsigned int>(eventInfo->eventID);
		signalData["gameObjID"] = static_cast<unsigned int>(eventInfo->gameObjID);
		signalData["playingID"] = static_cast<unsigned int>(eventInfo->playingID);
		break;
	}
	case AK_MusicSyncBeat:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncBar:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncEntry:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncExit:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncGrid:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncUserCue:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncPoint:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MusicSyncAll:
	{
		AkMusicSyncCallbackInfo* musicSyncInfo = static_cast<AkMusicSyncCallbackInfo*>(callbackInfo);
		signalData["gameObjID"] = static_cast<unsigned int>(musicSyncInfo->gameObjID);
		signalData["musicSyncType"] = static_cast<unsigned int>(musicSyncInfo->musicSyncType);
		signalData["playingID"] = static_cast<unsigned int>(musicSyncInfo->playingID);
		signalData["pszUserCueName"] = String(musicSyncInfo->pszUserCueName);

		Dictionary segmentInfo;
		segmentInfo["fBarDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBarDuration);
		segmentInfo["fBeatDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fBeatDuration);
		segmentInfo["fGridDuration"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridDuration);
		segmentInfo["fGridOffset"] = static_cast<float>(musicSyncInfo->segmentInfo.fGridOffset);
		segmentInfo["iActiveDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iActiveDuration);
		segmentInfo["iCurrentPosition"] = static_cast<int>(musicSyncInfo->segmentInfo.iCurrentPosition);
		segmentInfo["iPostExitDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPostExitDuration);
		segmentInfo["iPreEntryDuration"] = static_cast<int>(musicSyncInfo->segmentInfo.iPreEntryDuration);
		segmentInfo["iRemainingLookAheadTime"] = static_cast<int>(musicSyncInfo->segmentInfo.iRemainingLookAheadTime);
		signalData["segmentInfo"] = segmentInfo;
		break;
	}
	case AK_MIDIEvent:
	{
		AkMIDIEventCallbackInfo* midiEventInfo = static_cast<AkMIDIEventCallbackInfo*>(callbackInfo);
		signalData["eventID"] = static_cast<unsigned int>(midiEventInfo->eventID);
		signalData["gameObjID"] = static_cast<unsigned int>(midiEventInfo->gameObjID);


		Dictionary midiEvent;
		midiEvent["byType"] = static_cast<unsigned char>(midiEventInfo->midiEvent.byType);
		midiEvent["byChan"] = static_cast<unsigned char>(midiEventInfo->midiEvent.byChan);

		Dictionary cc;
		cc["byCc"] = static_cast<unsigned char>(midiEventInfo->midiEvent.Cc.byCc);
		cc["byValue"] = static_cast<unsigned char>(midiEventInfo->midiEvent.Cc.byValue);

		Dictionary chanAftertouch;
		chanAftertouch["byValue"] = static_cast<unsigned char>(midiEventInfo->midiEvent.ChanAftertouch.byValue);

		Dictionary gen;
		gen["byParam1"] = static_cast<unsigned char>(midiEventInfo->midiEvent.Gen.byParam1);
		gen["byParam2"] = static_cast<unsigned char>(midiEventInfo->midiEvent.Gen.byParam2);

		Dictionary noteAftertouch;
		noteAftertouch["byNote"] = static_cast<unsigned char>(midiEventInfo->midiEvent.NoteAftertouch.byNote);
		noteAftertouch["byValue"] = static_cast<unsigned char>(midiEventInfo->midiEvent.NoteAftertouch.byValue);

		Dictionary noteOnOff;
		noteOnOff["byNote"] = static_cast<unsigned char>(midiEventInfo->midiEvent.NoteOnOff.byNote);
		noteOnOff["byVelocity"] = static_cast<unsigned char>(midiEventInfo->midiEvent.NoteOnOff.byVelocity);

		Dictionary pitchBend;
		pitchBend["byValueLsb"] = static_cast<unsigned char>(midiEventInfo->midiEvent.PitchBend.byValueLsb);
		pitchBend["byValueMsb"] = static_cast<unsigned char>(midiEventInfo->midiEvent.PitchBend.byValueMsb);

		Dictionary programChange;
		pitchBend["byProgramNum"] = static_cast<unsigned char>(midiEventInfo->midiEvent.ProgramChange.byProgramNum);

		midiEvent["cc"] = cc;
		midiEvent["chanAftertouch"] = chanAftertouch;
		midiEvent["gen"] = gen;
		midiEvent["noteAftertouch"] = noteAftertouch;
		midiEvent["noteOnOff"] = noteOnOff;
		midiEvent["pitchBend"] = pitchBend;
		midiEvent["programChange"] = programChange;

		signalData["midiEvent"] = midiEvent;

		signalData["playingID"] = static_cast<unsigned int>(midiEventInfo->playingID);
		break;
	}
	case AK_CallbackBits:
	case AK_EnableGetSourcePlayPosition:
	case AK_EnableGetMusicPlayPosition:
	case AK_EnableGetSourceStreamBuffering:
		break;
	default:
		AKASSERT(false);
		break;
	}

	if (signalDataArray->size() < signalCallbackDataMaxSize)
	{
		signalDataArray->append(signalData);
	}
	else
	{
		Godot::print_warning("Exceeded the signal callback data maximum size (callback manager)",
							__FUNCTION__, __FILE__, __LINE__);
	}
}

void Wwise::emitSignals()
{
	AkAutoLock<CAkLock> ScopedLock(signalDataLock);

	const int initialArraySize = signalDataArray->size();

	for (int signalIndex = 0; signalIndex < initialArraySize; ++signalIndex)
	{
		const Dictionary data = signalDataArray->pop_front();
		const unsigned int callbackType = static_cast<unsigned int>(data["callbackType"]);

		emit_signal(WwiseCallbackToSignal(static_cast<AkCallbackType>(callbackType)), data);
	}

	AKASSERT(signalDataArray->size() == 0);
}

void Wwise::bankCallback(AkUInt32 bankID, const void* inMemoryBankPtr, AKRESULT loadResult, AkMemPoolId memPoolId)
{
	AkAutoLock<CAkLock> ScopedLock(signalDataLock);

	Dictionary signalData;
	signalData["bankID"] = static_cast<unsigned int>(bankID);
	signalData["result"] = static_cast<unsigned int>(loadResult);

	if (signalBankDataArray->size() < signalCallbackDataMaxSize)
	{
		signalBankDataArray->append(signalData);
	}
	else
	{
		Godot::print_warning("Exceeded the signal callback data maximum size (callback manager)",
							__FUNCTION__, __FILE__, __LINE__);
	}
}

void Wwise::emitBankSignals()
{
	AkAutoLock<CAkLock> ScopedLock(signalDataLock);

	const int initialArraySize = signalBankDataArray->size();

	for (int signalIndex = 0; signalIndex < initialArraySize; ++signalIndex)
	{
		const Dictionary data = signalBankDataArray->pop_front();
		emit_signal("bank_callback", data);
	}

	AKASSERT(signalBankDataArray->size() == 0);
}

Variant Wwise::getPlatformProjectSetting(const String setting)
{
	AKASSERT(projectSettings);
	AKASSERT(!setting.empty());

	String platformSetting = setting;

#ifdef AK_WIN
	platformSetting += GODOT_WINDOWS_SETTING_POSTFIX;
#elif defined(AK_MAC_OS_X)
	platformSetting += GODOT_MAC_OSX_SETTING_POSTFIX;
#elif defined(AK_IOS)
	platformSetting += GODOT_IOS_SETTING_POSTFIX;
#elif defined(AK_ANDROID)
	platformSetting += GODOT_ANDROID_SETTING_POSTFIX;
#elif defined(AK_LINUX)
	platformSetting += GODOT_LINUX_SETTING_POSTFIX;
#else
#error "Platform not supported"
#endif

	// Try to get the platform-specific setting, if it exists
	if (projectSettings && projectSettings->has_setting(platformSetting))
	{
		return projectSettings->get(platformSetting);
	}

	// Otherwise, try to get the default platform-agnostic setting
	if (projectSettings && projectSettings->has_setting(setting))
	{
		return projectSettings->get(setting);
	}
	else
	{
		AKASSERT(false);
		Godot::print("Failed to get setting " + platformSetting);
		return "";
	}
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

	deviceSettings.bUseStreamCache = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "use_stream_cache"));

	deviceSettings.fTargetAutoStmBufferLength = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "target_auto_stream_buffer_length_ms"));

	deviceSettings.uIOMemorySize = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "IO_memory_size"));

	deviceSettings.uMaxCachePinnedBytes = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "maximum_pinned_bytes_in_cache"));

	if (!ERROR_CHECK(lowLevelIO.Init(deviceSettings), "Low level IO failed"))
	{
		return false;
	}

	AkInitSettings initSettings;
	AK::SoundEngine::GetDefaultInitSettings(initSettings);

#if defined(AK_ENABLE_ASSERTS)
	initSettings.pfnAssertHook = WwiseAssertHook;
#endif

	initSettings.bDebugOutOfRangeCheckEnabled = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "debug_out_of_range_check_enabled"));

	initSettings.bEnableGameSyncPreparation = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "enable_game_sync_preparation"));

	initSettings.fDebugOutOfRangeLimit = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "debug_out_of_range_limit"));

	String audioDeviceShareSet = getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH +
									"main_output/audio_device_shareset");
	initSettings.settingsMainOutput.audioDeviceShareset = 
	AK::SoundEngine::GetIDFromString(audioDeviceShareSet.alloc_c_string());

	const unsigned int channelConfigType = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "main_output/channel_config/channel_config_type"));

	const unsigned int numChannels = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "main_output/channel_config/number_of_channels"));
	
	if (channelConfigType == AK_ChannelConfigType_Anonymous)
	{
		initSettings.settingsMainOutput.channelConfig.SetAnonymous(numChannels);
	}
	else if (channelConfigType == AK_ChannelConfigType_Standard)
	{
		initSettings.settingsMainOutput.channelConfig.SetStandard(numChannels);
	}
	else if (channelConfigType == AK_ChannelConfigType_Ambisonic)
	{
		initSettings.settingsMainOutput.channelConfig.SetAmbisonic(numChannels);
	}
	else
	{
		AKASSERT(false);
	}

	initSettings.settingsMainOutput.idDevice = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "main_output/device_id"));

	initSettings.settingsMainOutput.ePanningRule = 
	static_cast<AkPanningRule>(static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "main_output/panning_rule")));

	initSettings.uCommandQueueSize = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "command_queue_size"));

	initSettings.uContinuousPlaybackLookAhead = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "continuous_playback_look_ahead"));

	initSettings.uMaxHardwareTimeoutMs = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "maximum_hardware_timeout_ms"));

	initSettings.uMaxNumPaths = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "maximum_number_of_positioning_paths"));

	initSettings.uMonitorQueuePoolSize = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + "monitor_queue_pool_size"));

	const unsigned int numSamplesPerFrameEnum = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "samples_per_frame"));

	switch (numSamplesPerFrameEnum)
	{
		case SamplesPerFrame::SAMPLES_256:
			initSettings.uNumSamplesPerFrame = 256;
			break;
		case SamplesPerFrame::SAMPLES_512:
			initSettings.uNumSamplesPerFrame = 512;
			break;
		case SamplesPerFrame::SAMPLES_1024:
			initSettings.uNumSamplesPerFrame = 1024;
			break;
		case SamplesPerFrame::SAMPLES_2048:
			initSettings.uNumSamplesPerFrame = 2048;
		default:
			AKASSERT(false);
			break;
	}

	AkPlatformInitSettings platformInitSettings;
	AK::SoundEngine::GetDefaultPlatformInitSettings(platformInitSettings);

	// Common platform settings
	const unsigned int numRefillsInVoiceEnum = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "number_of_refills_in_voice"));

	switch (numRefillsInVoiceEnum)
	{
		case NumRefillsInVoice::REFILLS_2:
			platformInitSettings.uNumRefillsInVoice = 2;
			break;
		case NumRefillsInVoice::REFILLS_4:
			platformInitSettings.uNumRefillsInVoice = 4;
			break;
		default:
			AKASSERT(false);
			break;
	}

	const unsigned int sampleRateEnum =
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "sample_rate"));

	switch (sampleRateEnum)
	{
		case SampleRate::RATE_16000:
			platformInitSettings.uSampleRate = 16000;
			break;
		case SampleRate::RATE_24000:
			platformInitSettings.uSampleRate = 24000;
			break;
		case SampleRate::RATE_32000:
			platformInitSettings.uSampleRate = 32000;
			break;
		case SampleRate::RATE_44100:
			platformInitSettings.uSampleRate = 44100;
			break;
		case SampleRate::RATE_48000:
			platformInitSettings.uSampleRate = 48000;
			break;
		default:
			AKASSERT(false);
			break;
	}

	// Platform-specific settings
#ifdef AK_WIN
	platformInitSettings.bGlobalFocus = 
	static_cast<bool>(getPlatformProjectSetting("wwise/windows_advanced_settings/global_focus"));

	platformInitSettings.eAudioAPI = 
	static_cast<AkAudioAPI>(static_cast<unsigned int>(getPlatformProjectSetting("wwise/windows_advanced_settings/audio_API")));
#elif defined(AK_MAC_OS_X)

#elif defined(AK_IOS)
	const unsigned int sessionCategoryEnum =
	static_cast<unsigned int>(getPlatformProjectSetting("wwise/ios_advanced_settings/audio_session_category"));

	platformInitSettings.audioSession.eCategory = static_cast<AkAudioSessionCategory>(sessionCategoryEnum);

	const unsigned int sessionCategoryFlags =
	static_cast<unsigned int>(getPlatformProjectSetting("wwise/ios_advanced_settings/audio_session_category_options"));

	platformInitSettings.audioSession.eCategoryOptions = static_cast<AkAudioSessionCategoryOptions>(sessionCategoryEnum);

	const unsigned int audioSessionModeEnum =
	static_cast<unsigned int>(getPlatformProjectSetting("wwise/ios_advanced_settings/audio_session_mode"));

	platformInitSettings.audioSession.eMode = static_cast<AkAudioSessionMode>(audioSessionModeEnum);

#elif defined(AK_ANDROID)

	platformInitSettings.eAudioAPI = 
	static_cast<AkAudioAPIAndroid>(static_cast<unsigned int>(getPlatformProjectSetting("wwise/android_advanced_settings/audio_API")));

	platformInitSettings.bRoundFrameSizeToHWSize = 
	static_cast<bool>(getPlatformProjectSetting("wwise/android_advanced_settings/round_frame_size_to_hw_size"));

	JNIEnv* env = godot::android_api->godot_android_get_env();

	JavaVM* javaVM;
	env->GetJavaVM(&javaVM);
	platformInitSettings.pJavaVM = javaVM;

	jobject activity = godot::android_api->godot_android_get_activity();
	platformInitSettings.jActivity = activity;

	if (!ERROR_CHECK(lowLevelIO.InitAndroidIO(javaVM, activity), "Initialising Android IO failed"))
	{
		return false;
	}

#elif defined(AK_LINUX)

	platformInitSettings.eAudioAPI = static_cast<AkAudioAPILinux>(static_cast<unsigned int>(getPlatformProjectSetting("wwise/linux_advanced_settings/audio_API")));

#else
#error "Platform not supported"
#endif

	if (!ERROR_CHECK(AK::SoundEngine::Init(&initSettings, &platformInitSettings), "Sound engine initialisation failed"))
	{
		return false;
	}

	AkMusicSettings musicInitSettings;
	AK::MusicEngine::GetDefaultInitSettings(musicInitSettings);

	musicInitSettings.fStreamingLookAheadRatio = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + "streaming_look_ahead_ratio"));

	if (!ERROR_CHECK(AK::MusicEngine::Init(&musicInitSettings), "Music engine initialisation failed"))
	{
		return false;
	}

	AkSpatialAudioInitSettings spatialSettings;

	spatialSettings.uMaxSoundPropagationDepth = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "max_sound_propagation_depth"));
	
	spatialSettings.uDiffractionFlags = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "diffraction_flags"));

	spatialSettings.fDiffractionShadowAttenFactor = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "diffraction_shadow_attenuation_factor"));
	
	spatialSettings.fDiffractionShadowDegrees = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_ADVANCED_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "diffraction_shadow_degrees"));

	spatialSettings.fMovementThreshold = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "movement_threshold"));

	spatialSettings.uNumberOfPrimaryRays = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "number_of_primary_rays"));
	
	spatialSettings.uMaxReflectionOrder = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "max_reflection_order"));
	
	spatialSettings.fMaxPathLength = 
	static_cast<float>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "max_path_length"));

	spatialSettings.bEnableDiffractionOnReflection = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "enable_diffraction_on_reflections"));
	
	spatialSettings.bEnableDirectPathDiffraction = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "enable_direct_path_diffraction"));
	
	spatialSettings.bEnableTransmission = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMON_USER_SETTINGS_PATH + WWISE_SPATIAL_AUDIO_PATH + "enable_transmission"));

	if (!ERROR_CHECK(AK::SpatialAudio::Init(spatialSettings), "Spatial Audio initialisation failed"))
	{
		return false;
	}

#ifndef AK_OPTIMIZED
	AkCommSettings commSettings;
	AK::Comm::GetDefaultInitSettings(commSettings);

	commSettings.bInitSystemLib = 
	static_cast<bool>(getPlatformProjectSetting(WWISE_COMMUNICATION_SETTINGS_PATH + "initialize_system_comms"));

	commSettings.ports.uCommand = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMUNICATION_SETTINGS_PATH + "command_port"));

	commSettings.ports.uDiscoveryBroadcast = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMUNICATION_SETTINGS_PATH + "discovery_broadcast_port"));

	commSettings.ports.uNotification = 
	static_cast<unsigned int>(getPlatformProjectSetting(WWISE_COMMUNICATION_SETTINGS_PATH + "notification_port"));

	String networkName = getPlatformProjectSetting(WWISE_COMMUNICATION_SETTINGS_PATH + "network_name");
	AKPLATFORM::SafeStrCpy(commSettings.szAppNetworkName, networkName.alloc_c_string(), AK_COMM_SETTINGS_MAX_STRING_SIZE);

	ERROR_CHECK(AK::Comm::Init(commSettings), "Comm initialisation failed");
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