import sys
import json

# Optional: mapping from note names to CYNTH_FREQ_ constants
note_map = {
    "C4": "CYNTH_FREQ_C",
    "C#4": "CYNTH_FREQ_Cs",
    "D4": "CYNTH_FREQ_D",
    "D#4": "CYNTH_FREQ_Ds",
    "E4": "CYNTH_FREQ_E",
    "F4": "CYNTH_FREQ_F",
    "F#4": "CYNTH_FREQ_Fs",
    "G4": "CYNTH_FREQ_G",
    "G#4": "CYNTH_FREQ_Gs",
    "A4": "CYNTH_FREQ_A",
    "A#4": "CYNTH_FREQ_As",
    "B4": "CYNTH_FREQ_B",
    "C5": "CYNTH_FREQ_C",
    "C#5": "CYNTH_FREQ_Cs",
    "D5": "CYNTH_FREQ_D",
    "D#5": "CYNTH_FREQ_Ds",
    "E5": "CYNTH_FREQ_E",
    "F5": "CYNTH_FREQ_F",
    "F#5": "CYNTH_FREQ_Fs",
    "G5": "CYNTH_FREQ_G",
    "G#5": "CYNTH_FREQ_Gs",
    "A5": "CYNTH_FREQ_A",
    "A#5": "CYNTH_FREQ_As",
    "B5": "CYNTH_FREQ_B",
    "C6": "CYNTH_FREQ_C",
    "C#6": "CYNTH_FREQ_Cs",
    "D6": "CYNTH_FREQ_D",
    "D#6": "CYNTH_FREQ_Ds",
    "E6": "CYNTH_FREQ_E",
    "F6": "CYNTH_FREQ_F",
    "F#6": "CYNTH_FREQ_Fs",
    "G6": "CYNTH_FREQ_G",
    "G#6": "CYNTH_FREQ_Gs",
    "A6": "CYNTH_FREQ_A",
    "A#6": "CYNTH_FREQ_As",
    "B6": "CYNTH_FREQ_B",
}


def main():
    input_json = sys.stdin.read()
    data = json.loads(input_json)

    for note in data:
        CYNTH_name = note_map.get(note["name"], f'"{note["name"]}"')
        time = note["time"]
        velocity = note["velocity"] / 10.0
        duration = note["duration"]

        print("                           {")
        print(f"                             {CYNTH_name},")
        print(f"                             {time}f,")
        print(f"                             {duration}f,")
        print(f"                             {velocity}f,")
        print("                           },")


if __name__ == "__main__":
    main()
