"""Native playback UI. The application supplies its existing playback backend."""
from dataclasses import dataclass
from math import isfinite
from .common import ui


@dataclass
class MediaSnapshot:
    frame: object = None
    position: float = 0
    duration: float = 0
    volume: float = 1
    playing: bool = False
    muted: bool = False
    seekable: bool = False
    ready: bool = False
    title: str = ''
    error: str = ''


@dataclass
class MediaPlayerState:
    seek: float = 0
    seeking: bool = False


def media_player(identifier, state, backend, size=(0, 240)):
    i, c = ui(); data = backend.snapshot(); i.push_id(identifier); i.begin_child('##player', size)
    if data.title: c.heading(data.title, 3)
    if data.error: c.callout('error', data.error)
    frame = data.frame
    if frame and frame.texture and frame.width > 0 and frame.height > 0:
        available = i.get_content_region_avail(); width = max(c.dp(1), available.x); height = width*frame.height/frame.width
        limit = max(c.dp(1), available.y-c.dp(90))
        if height > limit: width, height = width*limit/height, limit
        texture = frame.texture if isinstance(frame.texture, i.ImTextureRef) else i.ImTextureRef(frame.texture)
        i.image(texture, (width, height), frame.uv_min, frame.uv_max)
    i.begin_disabled(not data.ready)
    if c.ghost_button(c.tr_label('Pause' if data.playing else 'Play')): backend.play(not data.playing)
    i.same_line()
    if c.ghost_button(c.tr_label('Unmute' if data.muted else 'Mute')): backend.mute(not data.muted)
    duration = max(0, min(315360000, data.duration)) if isfinite(data.duration) else 0
    position = min(duration, max(0, data.position)) if isfinite(data.position) else 0
    if not state.seeking: state.seek = position
    i.begin_disabled(not data.seekable or duration <= 0)
    _, state.seek = i.slider_float(c.tr_label('Position###seek'), state.seek, 0, duration, '%.1f s')
    state.seeking = i.is_item_active()
    if i.is_item_deactivated_after_edit(): backend.seek(max(0, min(duration, state.seek)))
    i.end_disabled()
    i.text_unformatted(f'{position:.1f} / {duration:.1f} s')
    volume = max(0, min(1, data.volume)) if isfinite(data.volume) else 1
    changed, volume = i.slider_float(c.tr_label('Volume'), volume, 0, 1, '%.2f')
    if changed: backend.volume(volume)
    i.end_disabled(); i.end_child(); i.pop_id()
