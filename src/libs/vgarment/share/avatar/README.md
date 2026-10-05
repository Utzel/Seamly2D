# Avatar body data

`body.dat` is the body the 3D View's avatar is made from: the MakeHuman base mesh, its joint helpers and the
shape targets for gender, age, muscle, weight, breast size and the measure modifiers.

## Source and license

The base mesh and targets come from [MPFB2](https://github.com/makehumancommunity/mpfb2), commit
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`. They are released under
[CC0 1.0](https://github.com/makehumancommunity/mpfb2/blob/master/LICENSE.ASSETS.md) by the MakeHuman team
(Data Collection AB, Joel Palmius, Jonas Hauquier).

How the targets are mixed (`src/libs/vgarment/body_model.cpp`) follows MPFB2's `targetservice.py` and
`macro.json`, which are GPLv3 like Seamly2D.

## Rebuilding

```bash
python scripts/avatar/build_avatar_data.py <download folder>
```

The script downloads the files it needs from the pinned commit (about 10 MB) and writes `body.dat`. The format is
described at the top of the script.
