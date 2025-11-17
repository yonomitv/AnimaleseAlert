#use GetDevices.py to get the correct device identifier
# your device will be duplicated with different API interfaces such as MME, WASAPI, WDM-KS
# Using the MME one for your mic would be safest
# You may see your audio device displayed like this
# 4 CABLE Output (VB-Audio Virtual , MME (8 in, 0 out)
# then the following line should be
# mic_id = "CABLE Output (VB-Audio Virtual , MME"
MIC_ID = ""

# The location of your extracted vosk asr model.
# https://alphacephei.com/vosk/models
# vosk-model-en-us-daanzu-20200905 is good and efficient
# download it, extract it inside the root project folder, and vosk_model should be the folder name
# vosk_model = "vosk-model-en-us-daanzu-20200905"
VOSK_MODEL = ""

# channel point reward ID 
# eg. "92af127c-7326-4483-a52b-b0da0be61c01" Leave empty quotes if you don't know.
REWARD_ID = "" 

# necessary if you don't know your animalese reward's id.
# it must be exactly like how your redemption is titled on twitch. assume case sensitive
# the text should be inside quotes
REWARD_TITLE = "Animalese"

# Given to you when you create your twitch dev app
TWITCH_CLIENT_ID = "" 

# You only need to fill this out if you don't know your twitch user id
# and want this program to query twitch for yours
TWITCH_CLIENT_SECRET = "" 

# The twitch token with channel:manage:redemptions permissions that you created
TWITCH_ACCESS_TOKEN = ""

# Set this if you know it, otherwise, if you filled out the TWITCH_CLIENT_ID and TWITCH_CLIENT_SECRET
# this program will query twitch's api for you and find it
# Input strictly the number, no quotes
TWITCH_USER_ID = None #eg. 123456789

# The time in seconds that you want your animalese STT to last
ANIMALESE_DURATION = 60

# how often do you want the script to check twitch for new animalese redemptions in seconds
CHECK_TWITCH_INTERVAL = 2

# Self explantory, shifts pitch of the audio, will also increase its speed
# 1 is no change (will not sound like animalese)
# 2 is default
PITCH_SHIFT = 2

# Silence between sentences in milliseconds 
# Divide this number by the pitch shift, and you have your true silence time in ms
SILENCE_BETWEEN_SENTENCES = 750

# let's you test out the alert offline if set to True
# will simulate receving redeems
ENABLE_OFFLINE_TESTING = True

# Not yet fully implemented, do not use it doesn't work.
# If you want to have only mod approved redeems to be played, set this to True
APPROVED_REDEEMS_ONLY = False
