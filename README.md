# The Photo Cycle
A multi-monitor screensaver that cycles your photos.

## Features
- Display time
- Fade time
- Pan-and-scan strength
- Multiple include paths
- Multiple exclude paths (e.g. "C:\My Photos\Receipts" or "C:\My Photos\Spicy nudes")
- Pause the animations with P
- Toggle caption with D (Date), L (Geo location) and F (Source folder)
- Open settings with C (config)
- Can be ran stand-alone for your viewing pleasure
- Flip forward and backward with arrow keys
- Always fill the screen, no matter the aspect ratio or zoom level
- An optional caption: date + folder of origin (e.g. Kopenhagen 22-07-2012)
- Location information for the photo
- Background color (for images with transparency)
- Synchronized cycle or one-by-one per monitor
- Option to only use a single screen (black on the rest)
- Settings dialog via Screen Save Settings
- A cool logo

## Technical
- Minimal resources
- Working preview in Screen Save Settings
- Date is scanned from EXIF info, then looks for a date in the filename, then goes for file creation date
- Location is taken from EXIF lat/lon, then cobbled from nominatim json (async)
- Font options for the caption: font, size, outline width, font color, ouline color
- Alt+Tab and the task bar only show one of the multiple windows
- Alt+Enter toggles full-screen mode
- Esc quits
- Made with the help of various A.I.s, mostly ChatGPT and Claude

## TODO
- The little preview shows the desktop when editing settings
- I could not get outlines to work easily in DirectWrite. If anyone is an expert, I'd like to hear it. Now I'm using the old-but-true hack of rendering the text 4 times with an offset. So don't make that outline width bigger than 5! :-)
- In some cases it seems like it's still running in the background, without any windows to be seen. Might be a quirk of being a screensaver?
- The X button only works on the main window (but you use ESC almost always anyway)
- The font doesn't scale with the screen size, which makes it potentially very big.
- If you use an image from your iCloud-for-Windows folder, it starts to dynamically download the image. This causes a serious stall. TODO: thread the loading and only display the image when it's done.
- Maybe add videos :-)

## Roadmap
I think this is a fine little program unless someone has a great idea.
Maybe add a "Heart/Like" 'L' key and show those more often?
Maybe add a "Exclude" key 'X' per photo?
