# Entegrasyon testi için sahte Shoutcast/ICY radyo sunucusu: python3 fake_radio.py (emaj.mp3 gerektirir)
import socket, threading, time
data=open('emaj.mp3','rb').read()
def handle(c):
    req=b''
    while b'\r\n\r\n' not in req:
        d=c.recv(1024)
        if not d: return
        req+=d
    path=req.split(b' ')[1].decode()
    try:
        if path=='/redir':
            c.sendall(b'HTTP/1.1 302 Found\r\nLocation: /icy\r\nContent-Length: 0\r\n\r\n'); return
        if path=='/html':
            c.sendall(b'HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<html></html>'); return
        if path=='/dead':
            c.close(); return
        hdr=b'ICY 200 OK\r\nicy-name: Test FM\r\ncontent-type: audio/mpeg\r\nicy-br: 128\r\n\r\n' if path=='/icy' else b'HTTP/1.0 200 OK\r\nContent-Type: audio/mpeg\r\n\r\n'
        c.sendall(hdr)
        pos=3333  # akışın ortasından başla
        c.sendall(data[pos:pos+64000]); pos+=64000  # ilk patlama (burst)
        while True:
            chunk=data[pos:pos+4000]
            if len(chunk)<4000: pos=0; chunk=data[:4000]
            c.sendall(chunk); pos+=4000
            time.sleep(4000/16000)
    except Exception: pass
    finally: c.close()
s=socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1); s.bind(('127.0.0.1',8770)); s.listen(20)
while True:
    c,_=s.accept(); threading.Thread(target=handle,args=(c,),daemon=True).start()
