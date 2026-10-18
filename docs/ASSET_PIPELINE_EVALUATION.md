# Asset pipeline evaluation

Status: evaluation candidates, with primary sources checked on 2026-10-17. These vendor capabilities have not passed this project's asset acceptance. Audio is lower priority. This note authorizes no integrations, installs, generation, credit use or purchases.

## Foundation and candidates

| Tool | Proposed project role | Evaluation boundary |
| --- | --- | --- |
| Blender | Production foundation for buildings, vehicles and infantry: complex modeling, materials, rigging, animation, cleanup and export. | Keep editable source and reviewed Python scripts reproducible. Its [features](https://www.blender.org/features/), [Python API](https://docs.blender.org/api/current/index.html) and [export manual](https://docs.blender.org/manual/en/latest/files/import_export/index.html) describe the underlying capabilities. |
| Community Blender MCP | Optional automation bridge for operating Blender and iterating scenes. | The [community project](https://github.com/ahujasid/mcp-for-blender) is separate from the Blender Foundation. A bridge does not replace Blender's modeling capabilities or asset review; reviewed Python scripts remain an alternative. No installation is selected. |
| Tripo | Candidate accelerator for infantry/organic shapes and detailed props. | Its [API documentation](https://developers.tripo3d.ai/en/docs/introduction) lists generation, editing and animation tasks. Suitability for this project is a proposal: topology, skinning, export and repeated-unit performance must be checked on actual outputs. |
| ElevenLabs SFX | Retain for later unit, weapon and UI sound experiments. | [Text-to-sound-effects API](https://elevenlabs.io/docs/api-reference/text-to-sound-effects/convert). No audio implementation now; review applicable output terms and provenance before shipping. |
| Stable Audio | Retain for later original music experiments. | [Current licensing guidance](https://kb.stability.ai/knowledge-base/stable-audio-commerical-and-usage-licensing). Record the exact service/model and applicable terms before shipping; hosted and self-hosted routes have different licensing contexts. |

## Consistency and processing order

Follow [art direction](ART_DIRECTION.md): faithful RA2 silhouettes and faction character, without the rejected childish RA3 look. Use approved reference images and shared materials/palette across the pilot. Approval of visual references does not itself authorize uploading proprietary images to a service; keep source permissions and provenance with each asset.

Use Blender to inspect proportions, topology, UVs, materials and pivots, then complete cleanup/remeshing before final rigging and animation. Tripo's [conversion documentation](https://docs.tripo3d.ai/export/conversion.html) states that mesh editing/remeshing can remove bones, weights and animation bindings, including remeshing during conversion. Recheck the animation pipeline after geometry changes. An exported file is not proof of usable skinning or game performance.

## Small pilot and pass criteria

The existing first blockout gate remains Tesla Reactor, Rhino Tank and Conscript together: normal RTS view, zoomed-out view and simple motion, followed by explicit approval before finishing meshes, textures, rigs or effects. Approval is not recorded.

After that gate, evaluate one building, one vehicle and one infantry through the production pipeline:

- **Building:** one agreed unit-production building, with a Soviet War Factory as a proposed subject. Confirm that choice before making another asset; the existing Tesla Reactor is a power building and remains the first-blockout subject.
- **Tank:** Rhino with separate hull, turret and barrel pivots; verify orientation, turning and aiming after UE5 import.
- **Infantry:** Conscript with a usable skeleton and walk/fire/death clips; inspect deformation and animation import after cleanup.

Show the trio in UE5 at actual RTS camera distance and zoom range. Check silhouette, relative scale, team-color readability, pivots, collision and selection footprint, LOD transitions, material slots/textures and animation behavior. Follow with many repeated moving/animated units alongside combat, bots, production and replication. Measure frame time, simulation/animation cost, memory and rendering/material cost on declared hardware before setting budgets; no army count or asset budget is accepted yet.

For each candidate record route/version, permitted reference source, generation/edit settings, attempts and authorized credit cost, cleanup/rigging/export time, accepted quality, remaining defects and UE results. Compare time to an accepted asset with Blender-only authoring before broader use. Save editable work, caches, exports and captures on the designated non-system drive; review binary/LFS size and budget before uploads.

## Reusable workflow and trial records

Use this sequence for later projects as well, keeping reusable scripts/validation and rights-cleared assets separate from RA2-specific content:

| Stage | Practical output and current evidence |
| --- | --- |
| Reference | Approved silhouettes, palette, scale and permitted source images. RA2 blockout approval remains pending; reference ownership does not establish upload/distribution rights. |
| Modeling or generation | Editable source and reproducible settings. Authored blockout work exists; Tripo suitability is planned evaluation. |
| Cleanup | Inspect topology, UVs, material slots, pivots and collision; remesh before final rigging. Existing geometry/import checks are partial evidence; generated-model cleanup time is unmeasured. |
| Rig and animation | Validate skeleton, weights and walk/fire/death clips after cleanup. Complete infantry animation integration remains planned. |
| UE import | Preserve scale, axes, pivots, materials, collisions and LODs. Static proof imports exist; a complete distinct infantry pipeline remains planned. |
| Gameplay and visuals | Connect assets to unit roles and commands; inspect RTS stills, zoom and motion. Generic match proofs exist; final art/scale and physical-control acceptance remain separate. |
| Performance acceptance | Repeat assets under combined movement, animation, combat, bots, production and replication. Record hardware/configuration and measured costs; large-army budgets remain unproven. |

Use [first-proof evidence](FIRST_PROOF.md) and later exact source/build/capture records for lessons from actual work. Distinguish observed results from proposed tooling. For every future trial retain provenance/terms, settings, time spent, defects, source/export hashes, import outcome, camera conditions and acceptance or rejection reason. An attractive generation preview alone does not complete the workflow.

## Terms and open decisions

Tripo Studio and API use [separate billing/credit systems](https://www.tripo3d.ai/help/api-plugins/tripo-studiotripo-api); do not infer API access or matching terms from a Studio subscription. The [commercial-use guidance](https://www.tripo3d.ai/help/privacy-policy/how-to-use-tripo-models-commercially) distinguishes free and paid users and refers to authoritative terms. Trial-tier output rights have **not** been verified for this project.

Before any service evaluation or distribution, establish the exact product/tier, input-upload permission, output rights, retention/privacy conditions and authorized spend. Record those terms with provenance; do not invent a project license or redistribute raw EA assets/music. Remaining choices are the production-building subject, whether MCP adds value over scripts, and whether measured Tripo cleanup time/quality justifies further use. Gameplay and the existing visual approval gate remain ahead of audio.
