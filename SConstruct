env = Environment(CXX = 'clang++',
                  CXXFLAGS = '--std=c++1y')
env.ParseConfig('pkg-config --cflags --libs portaudio-2.0')
debug = ARGUMENTS.get('debug', 0)
if int(debug):
    env.Append(CXXFLAGS = ' -g')
else:
    env.Append(CXXFLAGS = ' -O3')
env.Program(('pedal.cpp', 'audioobject.cpp'))
Default('pedal')
