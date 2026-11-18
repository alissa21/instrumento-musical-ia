"""Elige qué IA responde el chat: Gemini (gem.py) o ChatGPT (chatgpt.py).

Se elige con IA_CHAT en el archivo .env:
    IA_CHAT=gemini     (si no está, se usa Gemini)
    IA_CHAT=chatgpt

Los demás archivos escriben "from asistente import gem" y usan gem.hablar(), gem.leer(),
gem.voz_actual... sin importar cuál de las dos se eligió: las dos tienen las mismas funciones."""
import os

from dotenv import load_dotenv

load_dotenv()

if os.getenv("IA_CHAT", "gemini").strip().lower() == "chatgpt":
    import chatgpt as gem
    print("IA del chat: ChatGPT")
else:
    import gem
