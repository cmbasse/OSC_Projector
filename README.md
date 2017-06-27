# OSC_Projector
OSC control of an Epson projector using its serial port and an ESP8266.

### TODO
1. Fix returns from projector
 1. ~~Fix OSC lock~~
 2. ~~Fix status returns~~
2. ~~Fix incremental functions~~
### Future Updates
1. Add web status page
2. Add ability to change local ip on web page

### OSC Commands

| OSC Command | Values | Meaning |
|-----------------------------|------------|----------------------------------------|
| /projector/freeze | 0 | Unfrozen |
|  | 1 | Freeze |
|  | 2 | Freeze Status |
| /projector/shutter | 0 | Not Shuttered |
|  | 1 | Shuttered |
|  | 2 | Shutter Status |
| /projector/zoom | 0 | Zoom Min |
|  | 1 | Zoom Max |
|  | 2 | Zoom Status |
| /projector/zoom/increment | -9000 → -1 | Zoom Out for X Milliseconds |
|  | 1 → 9000 | Zoom In for X Milliseconds |
| /projector/focus | 0 | Focus Min |
|  | 1 | Focus Max |
|  | 2 | Focus Status |
| /projector/focus/increment | -9000 → -1 | Focus Out for X Milliseconds |
|  | 1 → 9000 | Focus In for X Milliseconds |
| /projector/h-lens | 0 | Horizontal Lens Min |
|  | 1 | Horizontal Lens Max |
|  | 2 | Horizontal Lens Status |
| /projector/h-lens/increment | -9000 → -1 | Horizontal Lens Out for X Milliseconds |
|  | 1 → 9000 | Horizontal Lens In for X Milliseconds |
| /projector/v-lens | 0 | Vertical Lens Min |
|  | 1 | Vertical Lens Max |
|  | 2 | Vertical Lens Status |
| /projector/v-lens/increment | -9000 → -1 | Vertical Lens Out for X Milliseconds |
|  | 1 → 9000 | Vertical Lens In for X Milliseconds |
| /projector/h-pos/increment | -255 → -1 | Horizontal Position Out X Times |
|  | 1 → 255 | Horizontal Position In X Times |
| /projector/v-pos/increment | -255 → -1 | Vertical Position Out X Times |
|  | 1 → 255 | Vertical Position In X Times |
| /projector/power | 0 | Power Off |
|  | 1 | Power On |
|  | 2 | Power Status |
