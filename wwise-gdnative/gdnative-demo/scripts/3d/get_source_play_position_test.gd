extends Node

export(AK.EVENTS._enum) var event = AK.EVENTS.MUSIC
export(AkUtils.AkCallbackType) var callback_type = AkUtils.AkCallbackType.AK_EnableGetSourcePlayPosition

var playing_id
var musicPlayStarted = false

func _ready():
	Wwise.connect(AkUtils.Signals.MUSIC_PLAY_STARTED, self, "_on_music_play_started")
	var registerResult = Wwise.register_game_obj(self, "Registering Event Test")
	print("Registering Event test: ", registerResult)
	
	playing_id = Wwise.post_event_id_callback(event, callback_type | AkUtils.AkCallbackType.AK_MusicPlayStarted, self)

func _on_music_play_started(_data):
	musicPlayStarted = true
	
func _process(_delta):
	if musicPlayStarted:
		var position = Wwise.get_source_play_position(playing_id, true)
		print(position)
