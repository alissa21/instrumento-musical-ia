"""Loopback-only local music dashboard. Run: python src/dashboard.py"""
import argparse
import base64
import binascii
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import tempfile
import threading
import webbrowser
import re
import io
import sys
from urllib.parse import unquote
import mido

from generation import inspect, generate, load, preview_notes
from delivery import save_bundle
from soundfont_audio import AudioCache, audio_status

ROOT = Path(__file__).resolve().parents[1]
CARPETA_PROYECTOS = ROOT/'proyectos'
LOCK = threading.Lock()
AUDIO_LOCK = threading.Lock()
AUDIO_CACHE = AudioCache()


def proyectos_guardados():
    """Nombres (sin .mid) de los proyectos de la app, los más nuevos primero."""
    if not CARPETA_PROYECTOS.is_dir():
        return []
    archivos = sorted(CARPETA_PROYECTOS.glob('*.mid'), key=lambda p: p.stat().st_mtime, reverse=True)
    return [p.stem for p in archivos]


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, data, kind='application/json'):
        body = json.dumps(data, ensure_ascii=False).encode() if kind == 'application/json' else data
        self.send_response(status)
        self.send_header('Content-Type', kind + ('; charset=utf-8' if kind.startswith('text/') else ''))
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        self.wfile.write(body)

    def local_request(self):
        expected = f'127.0.0.1:{self.server.server_port}'
        return self.headers.get('Host') == expected and self.headers.get('Origin', f'http://{expected}') == f'http://{expected}'

    def do_GET(self):
        if not self.local_request():
            return self.reply(403, {'error': 'Use the dashboard at its 127.0.0.1 address'})
        files = {'/': ('index.html', 'text/html'), '/app.js': ('app.js', 'text/javascript'), '/es.js': ('es.js', 'text/javascript'), '/ia.js': ('ia.js', 'text/javascript'),'/style.css': ('style.css', 'text/css')}
        if self.path in files:
            name, kind = files[self.path]
            return self.reply(200, (ROOT/'web'/name).read_bytes(), kind)
        # Proyectos guardados por la app (carpeta proyectos/), para probarlos sin buscar el archivo.
        if self.path == '/api/proyectos':
            return self.reply(200, {'proyectos': proyectos_guardados()})
        if self.path.startswith('/api/proyecto?nombre='):
            elegido = unquote(self.path.split('=', 1)[1])
            if elegido not in proyectos_guardados():   # solo nombres de la lista: nada fuera de la carpeta
                return self.reply(404, {'error': 'No existe ese proyecto'})
            datos = (CARPETA_PROYECTOS/(elegido+'.mid')).read_bytes()
            return self.reply(200, {'name': elegido+'.mid', 'midi': base64.b64encode(datos).decode()})
        if self.path == '/api/audio-status':
            return self.reply(200, audio_status())
        if re.fullmatch(r'/audio/[0-9a-f]{64}\.wav', self.path):
            with AUDIO_LOCK:
                path = AUDIO_CACHE.directory/self.path.rsplit('/', 1)[1]
                if path.is_file():
                    return self.reply(200, path.read_bytes(), 'audio/wav')
        self.reply(404, {'error': 'Not found'})

    def do_POST(self):
        if not self.local_request():
            return self.reply(403, {'error': 'Only same-origin local requests are accepted'})
        if self.path in ('/api/hablar', '/api/grabar-melodia'):
            return self.con_ia()
        if self.path not in ('/api/inspect', '/api/generate', '/api/render'):
            return self.reply(404, {'error': 'Not found'})
        try:
            size = int(self.headers.get('Content-Length', 0))
            if not 0 < size <= 2_800_000:
                raise ValueError('Request too large or empty')
            body = json.loads(self.rfile.read(size))
            if not isinstance(body, dict) or not isinstance(body.get('midi'), str):
                raise ValueError('A MIDI upload is required')
            data = base64.b64decode(body['midi'], validate=True)
            if self.path == '/api/render':
                # Generated songs may exceed the upload note-count limit. Bound
                # the actual render by duration and event count instead.
                midi = mido.MidiFile(file=io.BytesIO(data))
                if midi.type == 2 or midi.ticks_per_beat <= 0 or sum(len(t) for t in midi.tracks) > 50000:
                    raise ValueError('MIDI no compatible con la reproducción')
                if not AUDIO_LOCK.acquire(blocking=False):
                    return self.reply(409, {'error': 'Ya se está preparando un audio; esperá a que termine.'})
                try:
                    key, path = AUDIO_CACHE.get(data, midi)
                    return self.reply(200, {'url': '/audio/'+key+'.wav', **audio_status()})
                finally:
                    AUDIO_LOCK.release()
            if self.path == '/api/inspect':
                result = inspect(data)
                result['preview'] = preview_notes(load(data)[0])
                return self.reply(200, result)
            if not LOCK.acquire(blocking=False):
                return self.reply(409, {'error': 'A generation is already running; try again when it finishes'})
            try:
                midi, report = generate(data, body.get('settings', {}))
                with tempfile.TemporaryDirectory() as temp:
                    path = Path(temp)/'result.mid'
                    report_path = save_bundle(midi, report, path, data, report['request']['mode'])
                    result = {'midi': base64.b64encode(path.read_bytes()).decode(),
                              'report': json.loads(report_path.read_text(encoding='utf-8')),
                              'preview': preview_notes(midi), 'seconds': round(midi.length, 2)}
            finally:
                LOCK.release()
            self.reply(200, result)
        except (ValueError, TypeError, binascii.Error, EOFError, OSError) as exc:
            self.reply(400, {'error': str(exc)})
        except Exception:
            import traceback
            traceback.print_exc()
            self.reply(500, {'error': 'Generation failed unexpectedly. Details are in the local server terminal.'})


    def con_ia(self):
        """Hablar con Gemini (/api/hablar) y grabar una melodía (/api/grabar-melodia).
        El audio llega grabado desde el navegador, en WAV y en base64 (ver estudio_ia.py)."""
        import estudio_ia   # carga Gemini, Whisper y la voz recién cuando se usan
        try:
            size = int(self.headers.get('Content-Length', 0))
            if not 0 < size <= estudio_ia.MAX_AUDIO * 4 // 3 + 4_000_000:
                raise ValueError('La grabación es demasiado larga o llegó vacía.')
            body = json.loads(self.rfile.read(size))
            if not isinstance(body, dict):
                raise ValueError('Pedido no válido')
            if self.path == '/api/hablar':
                result = estudio_ia.hablar(body.get('audio'), body.get('texto'), body.get('midi'))
            else:
                if not isinstance(body.get('audio'), str):
                    raise ValueError('Falta la grabación')
                result = estudio_ia.grabar_melodia(body['audio'])
            self.reply(200, result)
        except (ValueError, binascii.Error) as exc:
            self.reply(400, {'error': str(exc)})
        except Exception:
            import traceback
            traceback.print_exc()
            self.reply(500, {'error': 'Falló la IA. Los detalles están en la terminal del panel.'})


def main():
    # Basic Pitch imprime emojis en sus avisos. Si la salida no es una consola UTF-8, Python
    # cortaba la grabación con un error de "charmap": así los caracteres raros se reemplazan.
    for salida in (sys.stdout, sys.stderr):
        salida.reconfigure(encoding='utf-8', errors='replace')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--no-browser', action='store_true')
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    url = f'http://127.0.0.1:{server.server_port}'
    print(f'Music studio: {url}\nFiles stay local. Ctrl+C to stop.', flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        AUDIO_CACHE.close()


if __name__ == '__main__':
    main()
