json11root = 'external/json11'
env = Environment(CXX = 'clang++',
                  CXXFLAGS = '--std=c++1y -I/usr/local/include -Iexternal/websocketpp -I{json11root}'.format(**globals()),
                  LIBPATH = '/usr/local/lib',
                  LIBS = ('boost_system', 'boost_filesystem')
)
env.ParseConfig('pkg-config --cflags --libs portaudio-2.0')
debug = ARGUMENTS.get('debug', 0)
if int(debug):
    env.AppendUnique(CXXFLAGS = ' -g')
else:
    env.AppendUnique(CXXFLAGS = ' -O3')
json11env = env.Clone()
json11 = json11env.Library('json11', ('/'.join((json11root, 'json11.cpp')),))
pedalenv = env.Clone()
pedalenv.AppendUnique(LIBS = json11)
pedalsrc = (
    'pedal.cpp',
    'audioobject.cpp',
    'webserver.cpp',
)
pedal = pedalenv.Program(pedalsrc)
Default(pedal)
