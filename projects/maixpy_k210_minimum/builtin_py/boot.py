from machine import UART
import utime
import uos
import ustruct

try:
    from machine import unique_id
except:
    unique_id = None

try:
    import sensor
except:
    sensor = None

UART_ID = UART.UARTHS
BAUD = 2000000       
READ_CHUNK = 512

MAGIC = b"\xA5\x5A"
VERSION = 0x01
FRAME_OVERHEAD = 7  # magic(2) + version(1) + len(2) + crc(2)

TYPE_GET_ID_REQ = 0x01
TYPE_GET_ID_RES = 0x02
TYPE_STREAM_START_REQ = 0x11
TYPE_STREAM_START_RES = 0x12
TYPE_STREAM_STOP_REQ = 0x13
TYPE_STREAM_STOP_RES = 0x14
TYPE_STREAM_IMAGE = 0x22
TYPE_MODEL_SAVE_BEGIN_REQ = 0x31
TYPE_MODEL_SAVE_BEGIN_RES = 0x32
TYPE_MODEL_SAVE_CHUNK_REQ = 0x33
TYPE_MODEL_SAVE_CHUNK_RES = 0x34
TYPE_MODEL_SAVE_END_REQ = 0x35
TYPE_MODEL_SAVE_END_RES = 0x36

RESULT_OK = 0x00
RESULT_FAIL = 0x01

DEFAULT_CHUNK_SIZE = 1024
STREAM_CHUNK_SIZE = 512
STREAM_DEFAULT_W = 160
STREAM_DEFAULT_H = 128
STREAM_DEFAULT_QUALITY = 60
STREAM_DEFAULT_FPS = 10
STREAM_MAX_FPS = 30

MAX_MODEL_SIZE = 10 * 1024 * 1024
MAX_PAYLOAD_SIZE = DEFAULT_CHUNK_SIZE + 16
BASE = "/flash/" if "flash" in uos.listdir("/") else "/"
MODEL_PATH = BASE + "model.kmodel"

uart = UART(UART_ID, BAUD, read_buf_len=4096)

st = {
    "buf": bytearray(),
    "receiving": False,
    "model_size": 0,
    "received_size": 0,
    "f": None,
    "camera_ready": False,
    "streaming": False,
    "stream_w": STREAM_DEFAULT_W,
    "stream_h": STREAM_DEFAULT_H,
    "stream_quality": STREAM_DEFAULT_QUALITY,
    "stream_fps": STREAM_DEFAULT_FPS,
    "stream_frame_id": 0,
    "stream_last_ms": 0,
}


def ensure_camera_ready():
    if sensor is None:
        return False

    if st["camera_ready"]:
        return True

    try:
        sensor.reset()
        sensor.set_pixformat(sensor.RGB565)
        sensor.set_framesize(sensor.QQVGA)
        try:
            sensor.set_vflip(0)
        except:
            pass
        try:
            sensor.set_hmirror(0)
        except:
            pass
        sensor.run(1)
    except:
        return False

    st["camera_ready"] = True
    return True

ensure_camera_ready()


def debug_line(msg):
    try:
        uart.write((msg + "\n").encode())
    except:
        pass


def crc16_modbus(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def pack_frame(payload):
    head = bytes([VERSION]) + ustruct.pack("<H", len(payload))
    crc = crc16_modbus(head + payload)
    return MAGIC + head + payload + ustruct.pack("<H", crc)


def send_payload(payload):
    try:
        uart.write(pack_frame(payload))
    except:
        pass


def send_result(res_type, seq, result, extra=b""):
    send_payload(bytes([res_type, seq, result]) + extra)


def close_file_silent():
    f = st["f"]
    if f is not None:
        try:
            try:
                f.flush()
            except:
                pass
            f.close()
        except:
            pass
    st["f"] = None


def reset_model_state():
    close_file_silent()
    st["receiving"] = False
    st["model_size"] = 0
    st["received_size"] = 0


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


def find_subseq(buf, pat):
    n = len(buf)
    m = len(pat)
    if m == 0 or n < m:
        return -1

    first = pat[0]
    last = n - m
    for i in range(last + 1):
        if buf[i] != first:
            continue

        ok = True
        for j in range(1, m):
            if buf[i + j] != pat[j]:
                ok = False
                break
        if ok:
            return i
    return -1


def image_to_jpeg_bytes(img, quality):
    candidates = []

    if hasattr(img, "compress"):
        try:
            compressed = img.compress(quality=quality)
            if compressed is not None:
                candidates.append(compressed)
        except:
            try:
                compressed = img.compress(quality)
                if compressed is not None:
                    candidates.append(compressed)
            except:
                pass

    candidates.append(img)

    for obj in candidates:
        if obj is None:
            continue
        if isinstance(obj, bytes):
            return obj
        if isinstance(obj, bytearray):
            return bytes(obj)

        if hasattr(obj, "to_bytes"):
            try:
                b = obj.to_bytes()
                if b is not None:
                    return bytes(b)
            except:
                pass

        if hasattr(obj, "bytearray"):
            try:
                b = obj.bytearray()
                if b is not None:
                    return bytes(b)
            except:
                pass

    return None


def send_stream_image(data):
    data_len = len(data)
    if data_len <= 0:
        return

    frame_id = st["stream_frame_id"]
    st["stream_frame_id"] = (frame_id + 1) & 0xFFFFFFFF

    chunk_cnt = data_len // STREAM_CHUNK_SIZE
    if data_len % STREAM_CHUNK_SIZE != 0:
        chunk_cnt += 1

    idx = 0
    offset = 0
    while offset < data_len:
        end = offset + STREAM_CHUNK_SIZE
        if end > data_len:
            end = data_len
        chunk = data[offset:end]

        body = (
            ustruct.pack("<I", frame_id)
            + ustruct.pack("<H", idx)
            + ustruct.pack("<H", chunk_cnt)
            + ustruct.pack("<H", len(chunk))
            + chunk
        )
        payload = bytes([TYPE_STREAM_IMAGE, 0x00]) + body
        send_payload(payload)

        idx += 1
        offset = end


def stream_tick():
    if not st["streaming"]:
        return

    interval_ms = 1000 // st["stream_fps"]
    now = utime.ticks_ms()
    if utime.ticks_diff(now, st["stream_last_ms"]) < interval_ms:
        return

    st["stream_last_ms"] = now

    if not ensure_camera_ready():
        st["streaming"] = False
        return

    try:
        img = sensor.snapshot()
    except:
        return

    if hasattr(img, "resize"):
        try:
            resized = img.resize(st["stream_w"], st["stream_h"])
            if resized is not None:
                img = resized
        except:
            pass

    jpeg = image_to_jpeg_bytes(img, st["stream_quality"])
    if jpeg is None:
        return

    send_stream_image(jpeg)


def get_device_id_32():
    if unique_id is None:
        return b"\x00" * 32

    try:
        uid = bytes(unique_id())
    except:
        return b"\x00" * 32

    if not uid:
        return b"\x00" * 32

    out = bytearray(32)
    for i in range(32):
        out[i] = uid[i % len(uid)]
    return bytes(out)


def handle_get_id(seq):
    payload = bytes([TYPE_GET_ID_RES, seq]) + get_device_id_32()
    send_payload(payload)


def handle_stream_start(seq, body):
    if len(body) < 6:
        extra = (
            ustruct.pack("<H", STREAM_DEFAULT_W)
            + ustruct.pack("<H", STREAM_DEFAULT_H)
            + bytes([STREAM_DEFAULT_QUALITY, STREAM_DEFAULT_FPS])
        )
        send_result(TYPE_STREAM_START_RES, seq, RESULT_FAIL, extra)
        return

    req_w = ustruct.unpack("<H", body[0:2])[0]
    req_h = ustruct.unpack("<H", body[2:4])[0]
    req_quality = body[4]
    req_fps = body[5]

    applied_w = req_w if req_w > 0 else STREAM_DEFAULT_W
    applied_h = req_h if req_h > 0 else STREAM_DEFAULT_H
    applied_quality = req_quality if req_quality > 0 else STREAM_DEFAULT_QUALITY
    applied_fps = req_fps if req_fps > 0 else STREAM_DEFAULT_FPS

    applied_w = clamp(applied_w, 32, 320)
    applied_h = clamp(applied_h, 32, 240)
    applied_quality = clamp(applied_quality, 1, 100)
    applied_fps = clamp(applied_fps, 1, STREAM_MAX_FPS)

    if not ensure_camera_ready():
        extra = (
            ustruct.pack("<H", applied_w)
            + ustruct.pack("<H", applied_h)
            + bytes([applied_quality, applied_fps])
        )
        send_result(TYPE_STREAM_START_RES, seq, RESULT_FAIL, extra)
        return

    st["streaming"] = True
    st["stream_w"] = applied_w
    st["stream_h"] = applied_h
    st["stream_quality"] = applied_quality
    st["stream_fps"] = applied_fps
    st["stream_last_ms"] = utime.ticks_ms()

    extra = (
        ustruct.pack("<H", applied_w)
        + ustruct.pack("<H", applied_h)
        + bytes([applied_quality, applied_fps])
    )
    send_result(TYPE_STREAM_START_RES, seq, RESULT_OK, extra)


def handle_stream_stop(seq):
    st["streaming"] = False
    send_result(TYPE_STREAM_STOP_RES, seq, RESULT_OK)


def handle_model_begin(seq, body):
    if len(body) < 7:
        send_result(TYPE_MODEL_SAVE_BEGIN_RES, seq, RESULT_FAIL, ustruct.pack("<H", DEFAULT_CHUNK_SIZE))
        return

    model_size = ustruct.unpack("<I", body[1:5])[0]
    meta_len = ustruct.unpack("<H", body[5:7])[0]

    if len(body) != 7 + meta_len:
        send_result(TYPE_MODEL_SAVE_BEGIN_RES, seq, RESULT_FAIL, ustruct.pack("<H", DEFAULT_CHUNK_SIZE))
        return

    if model_size <= 0 or model_size > MAX_MODEL_SIZE:
        send_result(TYPE_MODEL_SAVE_BEGIN_RES, seq, RESULT_FAIL, ustruct.pack("<H", DEFAULT_CHUNK_SIZE))
        return

    reset_model_state()
    try:
        st["f"] = open(MODEL_PATH, "wb")
    except:
        send_result(TYPE_MODEL_SAVE_BEGIN_RES, seq, RESULT_FAIL, ustruct.pack("<H", DEFAULT_CHUNK_SIZE))
        return

    st["receiving"] = True
    st["model_size"] = model_size
    st["received_size"] = 0

    send_result(TYPE_MODEL_SAVE_BEGIN_RES, seq, RESULT_OK, ustruct.pack("<H", DEFAULT_CHUNK_SIZE))


def handle_model_chunk(seq, body):
    if len(body) < 6 or (not st["receiving"]) or st["f"] is None:
        send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_FAIL)
        return

    offset = ustruct.unpack("<I", body[0:4])[0]
    data_len = ustruct.unpack("<H", body[4:6])[0]
    data = body[6:]

    if data_len != len(data):
        send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_FAIL)
        return
    if data_len > DEFAULT_CHUNK_SIZE:
        send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_FAIL)
        return
    if offset != st["received_size"]:
        send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_FAIL)
        return
    if st["received_size"] + data_len > st["model_size"]:
        send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_FAIL)
        return

    try:
        st["f"].write(data)
    except:
        reset_model_state()
        send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_FAIL)
        return

    st["received_size"] += data_len
    send_result(TYPE_MODEL_SAVE_CHUNK_RES, seq, RESULT_OK)


def handle_model_end(seq):
    if (not st["receiving"]) or st["f"] is None:
        send_result(TYPE_MODEL_SAVE_END_RES, seq, RESULT_FAIL)
        return

    ok = st["received_size"] == st["model_size"]
    reset_model_state()
    if ok:
        send_result(TYPE_MODEL_SAVE_END_RES, seq, RESULT_OK)
    else:
        send_result(TYPE_MODEL_SAVE_END_RES, seq, RESULT_FAIL)


def process_payload(payload):
    if len(payload) < 2:
        return

    msg_type = payload[0]
    seq = payload[1]
    body = payload[2:]

    if msg_type == TYPE_GET_ID_REQ:
        handle_get_id(seq)
        return
    if msg_type == TYPE_STREAM_START_REQ:
        handle_stream_start(seq, body)
        return
    if msg_type == TYPE_STREAM_STOP_REQ:
        handle_stream_stop(seq)
        return
    if msg_type == TYPE_MODEL_SAVE_BEGIN_REQ:
        handle_model_begin(seq, body)
        return
    if msg_type == TYPE_MODEL_SAVE_CHUNK_REQ:
        handle_model_chunk(seq, body)
        return
    if msg_type == TYPE_MODEL_SAVE_END_REQ:
        handle_model_end(seq)
        return


def parse_frames_from_buffer():
    buf = st["buf"]
    total = len(buf)
    offset = 0

    while True:
        if total - offset < FRAME_OVERHEAD:
            break

        idx_rel = find_subseq(buf[offset:], MAGIC)
        if idx_rel < 0:
            offset = total
            break

        offset += idx_rel

        if total - offset < FRAME_OVERHEAD:
            break

        if buf[offset + 2] != VERSION:
            offset += 1
            continue

        payload_len = ustruct.unpack("<H", bytes(buf[offset + 3:offset + 5]))[0]
        if payload_len > MAX_PAYLOAD_SIZE:
            offset += 2
            continue

        frame_len = FRAME_OVERHEAD + payload_len
        if total - offset < frame_len:
            break

        body = bytes(buf[offset + 2:offset + 5 + payload_len])
        crc_rx = ustruct.unpack("<H", bytes(buf[offset + 5 + payload_len:offset + 7 + payload_len]))[0]
        crc_calc = crc16_modbus(body)
        payload = bytes(buf[offset + 5:offset + 5 + payload_len])

        offset += frame_len

        if crc_rx != crc_calc:
            continue

        process_payload(payload)

    if offset > 0:
        st["buf"] = bytearray(buf[offset:])


while True:
    try:
        data = uart.read(READ_CHUNK)
        if data:
            st["buf"].extend(data)
            parse_frames_from_buffer()

        stream_tick()

        if not data:
            utime.sleep_ms(2)
    except Exception as e:
        debug_line("ERR EXC " + str(e))
        utime.sleep_ms(20)
