"""Bind document embeds to an existing optional WebBackend/CEF view."""
from dataclasses import dataclass, field
from .web import web_panel


@dataclass
class DocumentWebAdapter:
    backend: object = None
    event: object = None
    urls: dict = field(default_factory=dict)

    def draw(self, media, size):
        view = self.backend(media) if self.backend else None
        if view is None: return False
        known = self.urls.get(media.id)
        if known != (view, media.source):
            if not view.navigate(media.source): return False
            self.urls[media.id] = view, media.source
        for event in web_panel('document-'+str(media.id), view, size):
            if self.event: self.event(media, event)
        return True

    def clear(self): self.urls.clear()
