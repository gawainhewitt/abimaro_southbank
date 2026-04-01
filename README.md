# abimaro_southbank

## Audio File Preparation

WAV files exported from Logic Pro (and some other DAWs) contain metadata chunks that can cause issues with the Wav Trigger. Use this ffmpeg command to strip metadata and ensure clean 16-bit, 44.1kHz stereo WAV files:
```bash
# Convert and strip metadata from all WAV files in current directory
mkdir -p fixed
for file in *.wav; do
  ffmpeg -i "$file" -acodec pcm_s16le -ar 44100 -ac 2 -map_metadata -1 "fixed/$file"
done
```

**Required format for Wav Trigger:**
- 16-bit PCM
- 44.1kHz sample rate
- Stereo (2 channels)
- No metadata chunks
- Files must be named: `001.wav`, `002.wav`, `003.wav`, etc.

**Checking file format:**
```bash
soxi filename.wav
```
```mermaid
flowchart LR
    subgraph Buttons["External — IP66 push buttons"]
        B1[Button 1]
        B2[Button 2]
        B3[Button 3]
        B4[Button 4]
        B5[Button 5]
        B6[Button 6]
    end

    subgraph Box["Weatherproof enclosure"]
        subgraph Glands["Cable glands"]
            G1[Gland 1]
            G2[Gland 2]
            G3[Gland 3]
            G4[Gland 4]
            G5[Gland 5]
            G6[Gland 6]
            GP[Gland — mains]
            GA[Gland — audio]
        end

        PSU["Power supply 240V AC → 5V DC"]
        MCU[ESP32]
        WT[Wav Trigger]
        SD[(SD card)]

        G1 & G2 & G3 -->|GPIO| MCU
        G4 & G5 & G6 -->|GPIO| MCU
        GP --> PSU
        PSU -->|5V| MCU
        PSU -->|5V| WT
        MCU -->|UART| WT
        SD --- WT
        WT -->|line out| GA
    end

    B1 --> G1
    B2 --> G2
    B3 --> G3
    B4 --> G4
    B5 --> G5
    B6 --> G6

    MAINS(["Mains 240V"]) --> GP
    GA -->|line out| AMP(["To amplifier"])
```