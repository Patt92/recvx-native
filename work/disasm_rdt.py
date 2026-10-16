import struct

with open("data/rooms/RM_1090.RDT", "rb") as f:
    data = f.read()

event_offset = struct.unpack_from("<I", data, 28)[0]
header = struct.unpack_from("<I", data, event_offset)[0]
scd0 = event_offset + header
scd1 = event_offset + struct.unpack_from("<I", data, event_offset + 4)[0]

scripts = []
for p in range(8, header, 4):
    off = event_offset + struct.unpack_from("<I", data, event_offset + p)[0]
    scripts.append(off)

op_lengths = {
    0x00: 2, 0x01: 2, 0x02: 2, 0x03: 2, 0x04: 6, 0x05: 6, 0x06: 4, 0x07: 6,
    0x08: 4, 0x09: 4, 0x0a: 4, 0x0d: 4, 0x12: 2, 0x13: 4, 0x14: 4, 0x1f: 4,
    0x20: 4, 0x22: 4, 0x24: 4, 0x25: 10, 0x26: 2, 0x27: 2, 0x28: 2, 0x2a: 4,
    0x2c: 4, 0x2f: 4, 0x31: 2, 0x35: 4, 0x37: 2, 0x39: 4, 0x3a: 2, 0x3c: 4,
    0x3d: 4, 0x3e: 4, 0x3f: 4, 0x43: 4, 0x4b: 4, 0x4d: 4, 0x57: 2, 0x5c: 2, 0x5d: 6,
    0x60: 6, 0x61: 2, 0x62: 6, 0x63: 2, 0x65: 4, 0x68: 6, 0x69: 6, 0x70: 2,
    0x71: 2, 0x73: 22, 0x75: 4, 0x76: 4, 0x78: 14, 0x79: 6, 0x7a: 2, 0x7b: 10,
    0x7c: 10, 0x7d: 4, 0x81: 2, 0x83: 2, 0x86: 16, 0x87: 2, 0x89: 2, 0x8a: 2,
    0x8b: 12, 0x8c: 6, 0x91: 4, 0x93: 4, 0x94: 4, 0x95: 4, 0x97: 4, 0x99: 4,
    0x9a: 4, 0x9d: 4, 0xa0: 6, 0xa1: 2, 0xa4: 4, 0xa5: 4, 0xa6: 4, 0xad: 4,
    0xae: 2, 0xb1: 6, 0xb2: 4, 0xb7: 2, 0xb8: 2, 0xbc: 2, 0xbd: 2, 0xbe: 6,
    0xbf: 4, 0xc1: 2, 0xc2: 6, 0xc5: 2, 0xc7: 4, 0xc9: 4, 0xcb: 2, 0xcc: 4,
    0xce: 2, 0xcf: 2, 0xd0: 4, 0xd4: 4,
    0xf3: 1, 0xf4: 1, 0xf8: 4, 0xf9: 1, 0xfa: 4, 0xfb: 2, 0xfc: 2, 0xfd: 1, 0xfe: 1, 0xff: 2
}

def disasm_block(start, max_instructions=50):
    p = start
    for _ in range(max_instructions):
        if p >= len(data): break
        op = data[p]
        length = op_lengths.get(op, 2)
        args = list(data[p+1:p+length])
        line = f"  {p:06x} (rel={p-event_offset:04x}): {op:02x} "
        if op == 0x00:
            print(f"{line}End")
            break
        elif op == 0x01:
            rel = data[p+1]
            print(f"{line}IfelCk target=0x{p+2+rel:x} (rel={rel})")
        elif op == 0x02:
            rel = data[p+1]
            print(f"{line}ElseCk target=0x{p+rel:x} (rel={rel})")
        elif op == 0x03:
            print(f"{line}Endif")
        elif op == 0x04:
            m, bit, extra, mode = data[p+1], struct.unpack_from("<H", data, p+2)[0], data[p+4], data[p+5]
            print(f"{line}CkFlag m={m} bit={bit} extra={extra} mode={mode}")
        elif op == 0x05:
            m, bit, extra, mode = data[p+1], struct.unpack_from("<H", data, p+2)[0], data[p+4], data[p+5]
            print(f"{line}SetFlag m={m} bit={bit} mode={mode}")
        elif op == 0x06:
            k, mode, opand = data[p+1], data[p+2], data[p+3]
            print(f"{line}CmpB key={k} mode={mode} op={opand}")
        elif op == 0x07:
            e, field, mode, opand = data[p+1], data[p+2], data[p+3], struct.unpack_from("<h", data, p+4)[0]
            print(f"{line}CmpW ene={e} field={field} mode={mode} op={opand}")
        elif op == 0x14:
            packed = struct.unpack_from("<H", data, p+2)[0]
            lvl = packed & 0xff
            evt = packed >> 8
            print(f"{line}EvtStart lvl={lvl} evt={evt} (raw={args})")
        elif op == 0x1f:
            print(f"{line}DispMessage mode={data[p+1]} idx={data[p+2]} freeze={data[p+3]}")
        elif op == 0xbc:
            print(f"{line}TaskKill lvl={data[p+1]}")
        elif op == 0x69:
            print(f"{line}CommonControll cno={data[p+1]} b={args[1:]}")
        elif op == 0xb1:
            v0, v1, v2, v3, v4 = data[p+1:p+6]
            print(f"{line}ObjDposCk ene={v0} flg={v1} v2={v2} v3={v3}(spd) node={v4}")
        elif op == 0xfa:
            cnt = struct.unpack_from("<h", data, p+2)[0]
            print(f"{line}FOR cnt={cnt}")
        elif op == 0xfb:
            print(f"{line}NEXT")
        elif op == 0xfc:
            rel = data[p+1]
            print(f"{line}WHILE target=0x{p+rel:x} (rel={rel})")
        elif op == 0xfd:
            print(f"{line}EWHILE")
        elif op == 0xfe:
            print(f"{line}WAIT (1 frame)")
        elif op == 0xff:
            print(f"{line}EEVT (task end)")
            p += length
            break
        else:
            print(f"{line}op=0x{op:02x} args={args}")
        p += length

print("=== Event 8 ===")
disasm_block(scripts[8])
print("\n=== Event 9 ===")
disasm_block(scripts[9])
print("\n=== Event 6 ===")
disasm_block(scripts[6])
print("\n=== Event 7 ===")
disasm_block(scripts[7])
print("\n=== Event 13 ===")
disasm_block(scripts[13])
