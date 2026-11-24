# PlatformIO extra script voor de native testomgeving: link de C++-runtime statisch, zodat de
# testprogramma's niet afhankelijk zijn van welke libstdc++-DLL toevallig in het PATH staat.
Import("env")  # noqa: F821  (wordt door PlatformIO/SCons geleverd)

env.Append(LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"])  # noqa: F821
