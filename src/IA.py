import os
import whisper

# Load the desired model size (options: tiny, base, small, medium, large, turbo)
model = whisper.load_model("base")

# Transcribe the audio file
result = model.transcribe("grabacion.wav")

# Print the recognized text
print(result["text"])


