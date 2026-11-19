# PropertyField

`PropertyField` is a retained property row built from LotUI widgets. Its public
API has no CAD, platform, or Vulkan types. The host owns each value and applies
edits through callbacks.

- `ReadWrite`: shows a value and submits edits.
- `ReadOnly`: shows a selectable value and rejects typing, paste, cut, and
  changes. Copy works when the host installs a clipboard handler on `input()`.
- `WriteOnly`: starts blank, calls the value callback on submission, and clears
  its input immediately. The field does not retain or expose the submitted
  value; the host decides how to store it.
- `calculator = true`: accepts finite arithmetic using `+ - * / ( )` and
  decimal or exponent notation. Enter and the `=` button evaluate and submit.
  Invalid expressions leave the host value unchanged and invoke `onError`.
- `pointPicker = true`: the point button invokes `onPick`. The host starts its
  own asynchronous viewport pick, then calls `acceptPoint({x,y,z})`. LotUI
  formats the three coordinates and invokes the value callback. A cancelled
  pick needs no update.

In 3dEngine the adapter can start `CAD_EdGetPoint` from `onPick`. Its
`CAD_EdInputOptions::onDone` callback receives `CAD_EdInputResult::point` when
`status == CAD_ED_RESULT_NORMAL`. The adapter must check the selected object,
apply the position with `CAD_SetPosition`, and refresh the field from the engine
after a successful change. `CAD_EdGetPoint` also accepts typed coordinates and
the engine's object snaps. A busy or cancelled engine pick leaves the property
unchanged.

The standalone `cad_inspector_demo` shows all access modes, arithmetic, and a
small scene surface that maps clicks to world XY coordinates. Its `--self-test`
checks property propagation and a real pointer event; the separate
`LotGUI_Sample` copy builds against the installed SDK.
