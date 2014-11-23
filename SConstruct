env = Environment(CXX = 'clang++',
                  CXXFLAGS = '--std=c++11')
env.ParseConfig('pkg-config --cflags --libs portaudio-2.0')
env.Program(('pedal.cpp', 'audioobject.cpp'))
