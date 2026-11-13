# DSH-DEX enclosure print layout

The Fusion and STEP files lay out the seven original enclosure solids with space between them. Each STL contains one part with its lowest point on the bed plane. The PCB, battery, magnets, screws, and key ring go in after printing.

## Send to the print shop

Upload the seven STL files in `Print-STLs` individually. Order one of each at 100% scale in millimeters. Use the colors below. Ask the shop to keep the rail channels, magnet pockets, screw pilots, and registration pins clear when removing supports and finishing. The tray exports with its back down, the stationary frame with its rails up, and the carriers, panels, and emblem with their front faces down. The shop can adjust orientation and supports for its process.

| File | Color | Part |
| --- | --- | --- |
| 01-White-rear-tray.stl | White | Rear tray with key-ring eye |
| 02-Black-stationary-frame.stl | Black | PCB retainer and fixed rails |
| 03-Black-upper-carrier.stl | Black | Carrier for the red slider |
| 04-Black-lower-carrier.stl | Black | Carrier for the white slider |
| 05-Red-upper-half.stl | Red | Upper exterior panel |
| 06-White-lower-half.stl | White | Lower exterior panel |
| 07-White-center-emblem.stl | White | Center emblem |

## Hardware for assembly

- Four 3 mm diameter × 1.5 mm thick disc magnets
- Four M1.6 × 5 mm plastic thread-forming screws with heads up to 3.0 mm in diameter
- Two M2 × 4 mm travel-stop screws with cylindrical heads 3.8 mm in diameter and up to 2.0 mm high
- Plastic-compatible adhesive and a split ring
- Assembled PCB with OLED and a protected LiPo that fits the 24 × 19 × 4.8 mm tray pocket

## Put it together

1. Clear any support debris and dry-fit both carriers on the frame. The upper carrier goes on the **outer rails from the top**. The lower carrier goes on the **inner rails from the bottom**. Remove the carriers after checking the fit.
2. Glue the red and white panels to their matching carriers using the registration pegs. Glue the center emblem to the lower carrier. Keep adhesive out of the rail channels.
3. Check magnet polarity in each mating pair. Glue the magnets into their pockets at or just below the lips, using one pair per slider.
4. Connect the battery with the polarity verified. Fit it and the PCB into the rear tray. Line up the USB-C and power switch with their cutouts, then attach the stationary frame using the four M1.6 screws.
5. Slide the red and white assemblies onto their respective rails. Open both halves to expose the screw bosses. Install the two M2 travel-stop screws between each carrier's stop lugs. The red half travels up 13.6 mm and the white half travels down 19.2 mm.
6. Check that both halves slide smoothly and that the magnets catch when closed. Fit the split ring through the rear eye. Open the red half fully for IR battles.

Each STL was checked for a single watertight solid, correct scale, and bed-plane alignment. Print and dry-fit a first set before gluing. The final sliding fit depends on the print process.
