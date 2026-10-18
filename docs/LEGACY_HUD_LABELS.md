# Legacy ID and health label placement

The non-skirmish command proof drew each `#ID HP` label directly beside its unit, independently clamping and truncating text at the viewport. Labels could overlap each other and brackets; the default-controls capture shows the #3/#5 cluster. Skirmishes already use bounded placement.

This follow-up changes only legacy presentation. It reserves the existing panel and every fully visible bracket footprint, including stroke width, before placing labels. It sorts primary, other selected, then stable unit ID; measures the full label with the same font/scale used for drawing; and uses the existing 66-candidate linked placement search. A dark background supports contrast. Accepted labels avoid the panel, brackets, earlier labels and leaders. Failure omits the whole world label rather than shortening its identity. Partly clipped or panel-occluded glyphs are hidden rather than clamped into a border pile.

Primary ID and HP also appear in row eight of the existing legacy panel. Panel geometry and camera exclusion remain unchanged. Font placement was checked in the scoped 1280x720 engine captures below. This does not guarantee every dense world label can fit.

## Verified scope

At clean `655f72bcccc6f2d11653e1baf66ffb218455a52d`, a fresh editor build and the exact ten HUD layout native tests passed. Default controls, native Stop and representative skirmish Victory passed under their original assertions and bounds. Root and independent review inspected all eight fresh 1280x720 captures, including the formerly overlapping #3/#5 labels and primary ID/HP panel fallback.

Source tree `c24ef2587bd11480531b3db4faafc41ead0d8ef7` and the unchanged Config/project bind DLL `2ADB728A84D0CE06622C49CA38413172025CE3ECBD9A72AA4E301312484CE77B`. Later documentation-only publication changes preserve those engine inputs. Raw reports and captures remain private in ignored storage.

Two native groups call the production wrapper: cluster geometry at1280x720 and960x540, and failure/clipping cases. Supplied measured rectangles do not by themselves prove Canvas font measurements. Rendered acceptance here is1280x720 only; targeted960x540/edge coverage remains open. Independent source/API review found no blocking changes to the skirmish marker block, shared placement implementation or panel geometry.

The original build wrapper's observed 48.6-second completion was comfortably inside its900-second bound, but its historical parent ancestry discovery did not persist full parent lifetime records. That instrumentation limitation is retained; a subsequently corrected wrapper has separate focused policy tests. No evidence is retroactively upgraded.

This patch does not resolve dense-skirmish omissions, status-line truncation, physical input/focus or the broader issue #18. No command, simulation, art, installation or paid-service changes are included. Integrate after the validated Stop/cooperative changes, retain their original failures and check resulting engine-input identity before transferring scoped evidence.
