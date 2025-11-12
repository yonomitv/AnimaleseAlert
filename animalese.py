# Credit to henryishuman for the original animalese TTS project
# https://www.youtube.com/@henryishuman/
# https://www.youtube.com/watch?v=IKMjg2fEGgE
# 
# I (yonomitv) used it to make this twitch alert project

# You can test the output audio by running it with a string argument eg. py animalese.py "Hello I am testing" 
# if you run it without any arguments then it will only output when it sees a redeem happen
# py animalese.py

import os, sys, string, re
from pathlib import Path
import traceback
import time
import unicodedata
from datetime import datetime, timezone

# required to install with pip
from pydub import AudioSegment
from pydub.playback import play
from profanityfilter import ProfanityFilter
import requests
import config


REWARD_ID = config.REWARD_ID

REWARD_TITLE = config.REWARD_TITLE

TWITCH_CLIENT_ID = config.TWITCH_CLIENT_ID

TWITCH_CLIENT_SECRET = config.TWITCH_CLIENT_SECRET

TWITCH_ACCESS_TOKEN = config.TWITCH_ACCESS_TOKEN

TWITCH_USER_ID = config.TWITCH_USER_ID

CHECK_TWITCH_INTERVAL = config.CHECK_TWITCH_INTERVAL

APPROVED_REDEEMS_ONLY = config.APPROVED_REDEEMS_ONLY



# You don't need to mess with this
TWITCH_NORMAL_ACCESS_TOKEN = ""


TEMP_FILE_NAME = "temp.wav"
letter_graphs = [
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
    "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
    "w", "x", "y", "z", 
]

# this is a quick and dirty solution until I think about a better solution
digit_set = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"}

digraphs = [
    "ch", "sh", "ph", "th", "wh"
]

PITCH_SHIFT = config.PITCH_SHIFT
SILENCE_BETWEEN_SENTENCES = config.SILENCE_BETWEEN_SENTENCES

bebebese = "bebebese_slow"

    
def build_sentence(sentence, filename):
    sentence_wav = AudioSegment.empty()
    #sentence = sentence.lower()
    sentence = replace_swear_words(sentence)
    sentence = replace_parentheses(sentence)
    print("build_sentence")
    print(sentence)
    
    sentence_split = sentence.split()
    print(sentence_split)
    
    output_file = Path() / "NewAudio" / f"{filename}.txt"
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with output_file.open(mode = "w") as transcript_file:
        for word in sentence_split:
            i = 0
            while (i < len(word)):
                char = None
                if (i < len(word) - 1) and ((word[i] + word[i + 1]).lower() in digraphs):
                    char = word[i] + word[i + 1]
                    #print(f"we in digraphs i={i} len(word)={len(word)}")
                    i += 1
                elif word[i].lower() in letter_graphs:
                    char = word[i]
                elif word[i] in string.punctuation or word[i] in digit_set:
                    char = bebebese
                

                if char != None:
                    # might be a digit, but i should be converting that to text anyways
                    # TODO
                    if char == bebebese:
                        transcript_file.write(str(word[i]))
                        transcript_file.write("\n")
                    else:
                        transcript_file.write(str(char))
                        transcript_file.write("\n")
                    new_segment = AudioSegment.from_wav("letters/{}.wav".format(char.lower()))
                    if i != 0 and i != len(word) - 1:
                        new_segment = new_segment.strip_silence(50, -25, 50)
                        duration_ms = len(new_segment)
                        new_segment = new_segment[:(duration_ms / 3) * 2]
                        #print(f"Old duration: {duration_ms}; New Duration {len(new_segment)}")
                    #print(f"Duration: {(len(new_segment))}")
                    transcript_file.write(str(len(change_playback_speed(new_segment, PITCH_SHIFT))))
                    transcript_file.write("\n")
                    sentence_wav += new_segment
                if word[i] == "." or word[i] == "!" or word[i] == "?" :
                    silence_audio = AudioSegment.silent(SILENCE_BETWEEN_SENTENCES)
                    sentence_wav += silence_audio
                    lenny = change_playback_speed(silence_audio, PITCH_SHIFT)
                    transcript_file.write(" ")
                    transcript_file.write("\n")
                    #transcript_file.write(f"{SILENCE_BETWEEN_SENTENCES}")
                    transcript_file.write(f"{len(lenny)}")
                    transcript_file.write("\n")
                    #print("WE HERE")
                i += 1
            # insert space after words
            transcript_file.write(" ")
            transcript_file.write("\n")
            transcript_file.write("0")
            transcript_file.write("\n")

    return sentence_wav

def replace_swear_words(sentence):
    swear_words = ["fuck", "shit", "piss", "crap", "bugger"]
    for word in swear_words:
        #sentence = sentence.replace(word, "*"*len(word))
        sentence = re.sub(word, "*"*len(word), sentence, flags=re.IGNORECASE)
    return sentence

    

def replace_parentheses(sentence):
    while "(" in sentence or ")" in sentence:
        start = sentence.index("(")
        end = sentence.index(")")
        sentence = sentence[:start] + "*"*(end-start) + sentence[end+1:]

    return sentence

def change_playback_speed(sound, speed_change):
    #print(speed_change)
    #print(sound.frame_rate)
    sound_with_altered_frame_rate = sound._spawn(sound.raw_data, overrides={
        "frame_rate": int(sound.frame_rate * speed_change)
    })
    return sound_with_altered_frame_rate.set_frame_rate(sound.frame_rate)

# def build_and_say_sentence_with_voice(sentence, voice):
    # sound = build_sentence(sentence)
    # sound = change_playback_speed(sound, voice)
    # #sound = sound.speedup(2.5, 150, 25)
    # #play(sound)
    # return sound
    
def GenerateAnimaleseForAlertApp(text, redemption_id):
    # don't rely on this. It helps a little, but it's easily bypassed
    pf = ProfanityFilter()
    censored_text = pf.censor(text)
    #print(f"censored: {censored_text}")

    # hopefully no unicode funny business
    cleaned_censored_text = unicodedata.normalize('NFKD', censored_text)
    cleaned_censored_text = cleaned_censored_text.encode('ASCII', 'ignore').decode('ASCII')

    #print(f"ASCII: {cleaned_censored_text}")
    

    sound = build_sentence(cleaned_censored_text, redemption_id)
    sound = change_playback_speed(sound, PITCH_SHIFT)
    # we should have already created this folder by this point
    # and the ./ I think is fine.
    sound.export(f"./NewAudio/{redemption_id}.wav", format="wav")
    
    # Nothing needs to be written here, its existence is enough
    output_file = Path() / "NewAudio" / f"{redemption_id}_F.txt"
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with output_file.open(mode = "w") as F_File:
        pass

    return sound

def GetAccessToken():
    if not TWITCH_CLIENT_ID or not TWITCH_CLIENT_SECRET:
        print(f"TWITCH_CLIENT_ID or TWITCH_CLIENT_SECRET not set")
        return
    oauth_url = (
        "https://id.twitch.tv/oauth2/token?client_id="
        + TWITCH_CLIENT_ID
        + "&client_secret="
        + TWITCH_CLIENT_SECRET
        + "&grant_type=client_credentials"
    )
    resp = requests.post(oauth_url, timeout=15)
    global TWITCH_NORMAL_ACCESS_TOKEN
    TWITCH_NORMAL_ACCESS_TOKEN = resp.json()["access_token"]

def GetStreamerID(streamer):
    
    if not TWITCH_NORMAL_ACCESS_TOKEN:
        GetAccessToken()
        if not TWITCH_NORMAL_ACCESS_TOKEN:
            print(f"GetAccessToken() failed.")
            exit()
    user_data = None
    headers = {
        "Authorization": "Bearer " + TWITCH_NORMAL_ACCESS_TOKEN,
        "Client-ID": TWITCH_CLIENT_ID
    }
    resp = requests.get(
        "https://api.twitch.tv/helix/users?login=" + streamer,
        headers=headers
    )
    if resp.status_code != 200:
        print(f"GetStreamerID() failed. response code = {resp.status_code}")
        exit()
    user_data = resp.json()
    print(user_data)
    return user_data["data"]
     

# Validates a Twitch CLI token with scope
def ValidateToken():
    req_URL = "https://id.twitch.tv/oauth2/validate"
    headers = {
        "Authorization": "Bearer " + TWITCH_ACCESS_TOKEN
    }
    resp = requests.get(req_URL, headers=headers)
    if resp.status_code == 401:
        return False
    return True

def GetCustomRewardID():
    headers = {
        "Authorization": "Bearer " + TWITCH_ACCESS_TOKEN,
        "Client-ID": TWITCH_CLIENT_ID
    }
    resp = requests.get(
        f"https://api.twitch.tv/helix/channel_points/custom_rewards?broadcaster_id={TWITCH_USER_ID}",
        headers=headers
    )
    if resp.status_code != 200:
        print(f"GetCustomRewardID() failed. response code = {resp.status_code}")
        if not ValidateToken():
            print(f"Your access token is invalid. Make a new on with Twitch CLI")
        exit()
    redemptions = resp.json()["data"]
    for redemption in redemptions:
        if redemption["title"] == REWARD_TITLE:
            global REWARD_ID
            REWARD_ID = redemption["id"]
            print(f"Reward id = {REWARD_ID}")
            return True
    print(f"GetCustomRewardID() failed to find reward ID")
    exit()
    
def GetRedemptions():
    headers = {
        "Authorization": "Bearer " + TWITCH_ACCESS_TOKEN,
        "Client-ID": TWITCH_CLIENT_ID
    }
    resp = requests.get(
        f"https://api.twitch.tv/helix/channel_points/custom_rewards/redemptions?broadcaster_id={TWITCH_USER_ID}&reward_id={REWARD_iD}&status=UNFULFILLED",
        headers=headers
    )
    if resp.status_code != 200:
        print(f"GetRedemptions() failed. response code = {resp.status_code}")
        if not ValidateToken():
            print(f"Your access token is invalid. Make a new on with Twitch CLI")
        exit()
    redeem_data = resp.json()["data"]
    return redeem_data


def main():
    # test mode
    if len(sys.argv) > 1:
        sentence = sys.argv[1]

        GenerateAnimaleseForAlertApp(sentence, "test")
    # Twitch Alert Mode
    else:
        print("No text to speak provided. Running in Twitch Alert Mode")
        # Check if channel point reward 
        if not TWITCH_CLIENT_ID:
            print("Your TWITCH_CLIENT_ID was not set. Try again when you have set it")
            return
        if not TWITCH_USER_ID:
            str_one = input("Your TWITCH_USER_ID was not set. Enter 0 if you'd like to find your ID ")
            if str_one == "0":
                user_str = input("Please enter your twitch username: ")
                user_str = user_str.lower()
                if user_str:
                    userdata = GetStreamerID(user_str)
                    print(f"{user_str}'s ID = {userdata[0]['id']}")
                    print("Please edit this file so that TWITCH_USER_ID is set to this ID value")
                    return
            return
        # setup program
        if not TWITCH_ACCESS_TOKEN:
            print("Your TWITCH_ACCESS_TOKEN was not set. Try again when you have set it")
            return
        
        if not REWARD_ID:
            if not REWARD_TITLE:
                print("Since you do not have the reward id, you need to set REWARD_TITLE")
                exit()
            if not GetCustomRewardID():
                print("Could not find matching reward id to reward title")
            
        
        output_file = Path() / f"Completed_Redemptions.txt"
        completed_folder = Path() / "CompletedAudio"
        NewAlertsNotRan = set()
        AlertsAlreadyRan = set()

        # initializes AlertsAlreadyRan with previously completed redemptions
        # Twitch returns redemptions for 3-4 days according to the API
        # I'm unsure if the id's are guaranteed unique beyond that date
        # I imagine any mixup would be very rare though if they're not
        with output_file.open("r") as completed_file:
            for line in completed_file:
                line_split = line.strip().split()
                if len(line_split) != 3:
                    continue
                # line_split[0] == redemption id
                # line_split[1] == date (for logging and potentially duplicate handling if needed)
                # line_split[2] == timestamp (for logging and potentially duplicate handling if needed)
                # redemption id's are very long so this check should be trivial.
                if len(line_split[0]) > 0:
                    AlertsAlreadyRan.add(line_split[0])

        with output_file.open("a+") as completed_file:
            while True:
                # check completed folder
                # Anything not logged in Completed_Redemptions.txt will be logged now.
                # If you have alerts that were generated in a previous stream but played in another
                # They will not be marked as fulfilled automatically on twitch
                # You will have to mark it yourself
                for entry in completed_folder.iterdir():
                    if entry.is_file() and entry.suffix == ".wav" and entry.stem not in AlertsAlreadyRan:
                        if entry.stem in NewAlertsNotRan:
                            if not APPROVED_REDEEMS_ONLY:
                                headers = {
                                    "Authorization": "Bearer " + TWITCH_ACCESS_TOKEN,
                                    "Client-ID": TWITCH_CLIENT_ID,
                                    "Content-Type": "application/json"
                                }
                                # i'm unsure on if this works.
                                r = requests.patch(
                                        f"https://api.twitch.tv/helix/channel_points/custom_rewards/redemptions?broadcaster_id={TWITCH_USER_ID}&reward_id={entry.stem}", 
                                        headers=headers, 
                                        json={"status": "FULFILLED"}
                                )
                                if r.status_code != 200:
                                    print(f"Attemped to set {entry.stem} redemption () failed. response code = {r.status_code}")
                                    exit()
                            NewAlertsNotRan.remove(entry.stem)
                        AlertsAlreadyRan.add(entry.stem)                        
                        now = datetime.now(timezone.utc)
                        date_time = now.strftime("%Y-%m-%d %H:%M:%S")
                        completed_file.write(f"{entry.stem} {date_time}\n")
                
                Unfulfilled = GetRedemptions()
                for Redeem in Unfulfilled:
                    dialogue_text = Redeem["user_input"]
                    # make sure we're not duplicating already handled alerts 
                    # and that the alerts were not already ran in a previous session.
                    # these are unhandled, unfulfilled redeems.

                    if dialogue_text and (Redeem["id"] not in NewAlertsNotRan or Redeem["id"] not in AlertsAlreadyRan):
                        GenerateAnimaleseForAlertApp(dialogue_text, Redeem["id"])
                        # I'm assuming twitch returns unique id's
                        NewAlertsNotRan.add(Redeem["id"])
                    # ideally you don't hit this and you require text for the alert when you make it
                    else:
                        print("No text provided. cancelling redeem")
                        headers = {
                            "Authorization": "Bearer " + TWITCH_ACCESS_TOKEN,
                            "Client-ID": TWITCH_CLIENT_ID,
                            "Content-Type": "application/json"
                        }
                        r = requests.patch(
                                f"https://api.twitch.tv/helix/channel_points/custom_rewards/redemptions?broadcaster_id={TWITCH_USER_ID}&reward_id={Redeem['id']}", 
                                headers=headers, 
                                json={"status": "CANCELED"}
                        )
                        if r.status_code != 200:
                            print(f"Attemped to set {entry.stem} redemption () failed. response code = {r.status_code}")
                            exit()
                time.sleep(CHECK_TWITCH_INTERVAL)


if __name__=="__main__":
    try:
        main()
    except Exception as e:
        print(e)
        print(traceback.format_exc())

        print("Program stopped because of error")
