extends Node

export(AK.EVENTS._enum) var event = AK.EVENTS.MUSIC

export(AK.TRIGGERS._enum) var trigger = AK.TRIGGERS.MUSICTRIGGER

func _ready():
	var registerResult = Wwise.register_game_obj(self, "Trigger Test")
	print("Registering Event test: ", registerResult)
	
	Wwise.post_event_id(event, self)

func _input(_ev):
	if Input.is_key_pressed(KEY_SPACE):
		var triggerResult = Wwise.post_trigger_id(trigger, self)
		print("Posting Trigger result: ", triggerResult)
