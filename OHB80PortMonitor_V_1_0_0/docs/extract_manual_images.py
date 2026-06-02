import json, base64, re, pathlib
from PIL import Image

session = pathlib.Path(r"C:\Users\Lenovo\.codex\sessions\2026\05\27\rollout-2026-05-27T17-25-26-019e68c0-bae0-7ba0-92cf-7209537d44c1.jsonl")
outdir = pathlib.Path(r"D:\Project\CYTC_Project\OHB80PortMonitor\OHB80PortMonitor_V_1_0_0\docs\manual_assets")
outdir.mkdir(parents=True, exist_ok=True)

saved = []
with session.open('r', encoding='utf-8') as f:
    for line in f:
        try:
            obj = json.loads(line)
        except Exception:
            continue
        if obj.get('type') != 'event_msg':
            continue
        p = obj.get('payload', {})
        if p.get('type') != 'user_message':
            continue
        msg = p.get('message', '')
        if '1~9' not in msg:
            continue
        imgs = p.get('images', []) or []
        for i, data_url in enumerate(imgs, start=1):
            m = re.match(r'^data:image/(png|jpeg|jpg);base64,(.*)$', data_url, re.I|re.S)
            if not m:
                continue
            ext = 'png' if m.group(1).lower() == 'png' else 'jpg'
            raw = base64.b64decode(m.group(2))
            path = outdir / f"img_{i:02d}.{ext}"
            path.write_bytes(raw)
            with Image.open(path) as im:
                w,h = im.size
            saved.append((str(path), w, h, len(raw)))
        break

for p,w,h,n in saved:
    print(f"{p}\t{w}x{h}\t{n}")
print(f"TOTAL={len(saved)}")
