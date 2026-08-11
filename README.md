# Stepwise drill distance calculator

> A new native C++/Qt drill-writing prototype is available in [`native/`](native/README.md). The original browser calculator below remains available as a reference and coordinate-data validator.

A zero-build browser app that calculates each performer's straight-line marching distance from the four included coordinate-sheet PDFs.

## Run

```powershell
python -m http.server 8000
```

Then open `http://localhost:8000`.

To regenerate the extracted data after replacing the PDFs:

```powershell
python extract_coordinates.py
```

## Calculation model

- Coordinate-sheet positions are mapped to an 8-to-5 step grid (8 steps = 5 yards).
- Each transition is the Euclidean distance between consecutive sets.
- Repeated coordinates are holds and add zero distance.
- High-school hash geometry is the default. College and NFL presets are available in the interface.
- The result is a straight-line estimate; curvilinear paths and choreography are not encoded in coordinate sheets.
- T07, T14, and T29 are excluded because they were cut from the show.
