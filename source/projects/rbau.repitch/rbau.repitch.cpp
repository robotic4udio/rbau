/// @file
///	@ingroup 	robotic-audio 
///	@copyright	Copyright 2025 Hjalte Bested Hjorth. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <set>
#include "Scale.h"
#include "Chord.h"
#include "Note.h"
#include "Interval.h"
#include "Arp.h"

using namespace c74::min;

// A class to represent a note. A note that can be repitched, therefore we store both pitch_in and pitch_out.


class LiveSet {
public:
    LiveSet() = default;

    // Setters
    void set_tempo     (double a_tempo   ) { tempo      = a_tempo;      }
    void set_beats     (double a_beats   ) { beats      = a_beats;      }
    void set_is_playing(bool a_is_playing) { is_playing = a_is_playing; }

    // Getters
    double get_tempo()      const { return tempo; }
    double get_beats()      const { return beats; }
    bool   get_is_playing() const { return is_playing; }

private:    
    double tempo = 120.0;      // The Tempo of the Live Set, in BPM.
    double beats = -1.0;       // The current playing position in the Live Set, in beats.
    bool   is_playing = false; // Is Live's transport is running?
};

class ChordTrack {
public:
    struct TimedChord {
    public:
        TimedChord() : m_chord(""), m_start(-1), m_end(-1) {}
        TimedChord(std::string a_chord, double a_start, double a_end) : m_chord(a_chord), m_start(a_start), m_end(a_end) {}
        cmtk::Chord m_chord;
        double      m_start;
        double      m_end;

        // Init
        void init() {
            m_chord = cmtk::Chord();
            m_start = -1;
            m_end   = -1;
        }

        // isValid
        bool isValid() const {
            if(m_start < 0)     return false;
            if(m_end < 0)       return false;
            if(m_start > m_end) return false;
            return m_chord.size() > 0;
        }

        // Compare two TimedChords
        bool operator==(const TimedChord& other) const { return m_start == other.m_start && m_end == other.m_end && m_chord == other.m_chord; }
        bool operator!=(const TimedChord& other) const { return !(*this == other); }

        // Stream operator to print the chord
        friend std::ostream& operator<<(std::ostream& os, const TimedChord& chord) {
            os << "TimedChord:(" << chord.m_chord << "," << chord.m_start << "," << chord.m_end << ")";
            return os;
        }
    };

    ChordTrack() {}

    void from_atoms(const atoms& args) {
        m_chords.clear();
        m_chords.reserve(args.size()/3);
        for(int i=0; i<args.size(); i+=3) {
            add_chord(args[i], args[i+1], args[i+2]);
        }
    }

    void add_chord(const std::string& a_chord, double a_start, double a_end) {
        m_chords.push_back(TimedChord(a_chord, a_start, a_end));
    }

    // Get TimedChord at time
    const TimedChord& get_chord_at_time(double time) const {
        for(auto it = m_chords.rbegin(); it != m_chords.rend(); ++it) {
            if(it->m_start <= time) return *it;
        }
        // Return an empty chord if no chord is found
        return NoChord;
    }

    // Stream operator to print the chord track
    friend std::ostream& operator<<(std::ostream& os, const ChordTrack& s_chord_track) {
        os << "ChordTrack:(";
        for(auto& chord : s_chord_track.m_chords) {
            os << chord << ",";
        }
        os << ")";
        return os;
    }


private:
    std::vector<TimedChord> m_chords;
    const TimedChord NoChord = TimedChord("", -1, -1);
};


class Note {
public:
    Note(int a_pitch_in, int a_pitch_out, int a_velocity);

    long id() { return m_id; }

    int         m_pitch_in;  // pitch we keep for the noteoff
    int         m_pitch_out; // pitch we keep for the noteon
    int         m_velocity;  // velocity of the note

    long        m_id;        // unique serial number for each note

    static long s_counter;

    // Implement a sorting operator for the set
    bool operator<(const Note& other) const {
        return m_pitch_out < other.m_pitch_out;
    }

    // Equality operator to use for deletion etc.
    bool operator==(const Note& other) const {
        return m_pitch_out == other.m_pitch_out;
    }

    // Print the note on a stream
    friend std::ostream& operator<<(std::ostream& os, const Note& note) {
        os << "Note: " << note.m_pitch_in << " " << note.m_pitch_out << " " << note.m_velocity;
        return os;
    }

};



class Springs {
public:
    class Spring {
    public:
        Spring() = default;
        // Index of this spring
        int index = -1;
        // Parameters
        int midinote = -1;
        int inst = 0;
        std::string mode = "midinote"; // Options: midinote, quantize, step, bass, root, high, low, rand, arp, chord, lowest 2, lowest 
        int note = 60;
        int transpose = 0;
        int pitch_min = 24;
        int pitch_max = 96;
        int octave_add = 0;
        int velocity = 90;
        int velocity_deviation = 10;
        int duration = 250;

        // Arp
        cmtk::ChordArp arp;
        int arp_reset = 0; // 0: Off, 1: Chord, 2: Bar, 3: Beat, 4: Bar & Chord, 5: Beat & Chord

        // Get the velocity with deviation
        int getVelocity() const {
            int v = velocity;

            if(velocity_deviation != 0) {
                v += (rand() % (2*velocity_deviation)) - velocity_deviation;
            }

            if(v < 0) return 0;
            if(v > 127) return 127;
            return v;
        }

    };

    Springs() = default;

    Spring& getSpring(int index) {
        Spring& spring = springs[index];
        spring.index = index;

        std::string index_str = std::to_string(index);

        // Get midinote value
        std::string midinote_str = index_str + "-midinote";
        atom midinote_value = param_dict[symbol(midinote_str)];
        spring.midinote = midinote_value;

        // Get inst
        std::string inst_str = index_str + "-inst";
        atom inst_value = param_dict[symbol(inst_str)];
        spring.inst = inst_value;

        // Get mode value
        std::string mode_str = index_str + "-mode";
        atom mode_value = param_dict[symbol(mode_str)];
        spring.mode = static_cast<std::string>(mode_value);

        // Get note
        std::string note_str = index_str + "-note";
        atom note_value = param_dict[symbol(note_str)];
        spring.note = note_value;

        // Get transpose
        std::string transpose_str = index_str + "-transpose";
        atom transpose_value = param_dict[symbol(transpose_str)];
        spring.transpose = transpose_value;

        // Get pitch_min value
        std::string pitch_min_str = index_str + "-pitch_min";
        atom pitch_min_value = param_dict[symbol(pitch_min_str)];
        spring.pitch_min = pitch_min_value;

        // Get pitch_max value
        std::string pitch_max_str = index_str + "-pitch_max";
        atom pitch_max_value = param_dict[symbol(pitch_max_str)];
        spring.pitch_max = pitch_max_value;

        // Get octave_add
        std::string octave_add_str = index_str + "-octave_add";
        atom octave_add_value = param_dict[symbol(octave_add_str)];
        spring.octave_add = octave_add_value;

        // Get velocity
        std::string velocity_str = index_str + "-velocity";
        atom velocity_value = param_dict[symbol(velocity_str)];
        spring.velocity = velocity_value;

        // Get velocity deviation
        std::string velocity_deviation_str = index_str + "-velocity_deviation";
        atom velocity_deviation_value = param_dict[symbol(velocity_deviation_str)];
        spring.velocity_deviation = velocity_deviation_value;

        // Get duration
        std::string duration_str = index_str + "-duration";
        atom duration_value = param_dict[symbol(duration_str)];
        spring.duration = duration_value;

        // Get arp_style
        std::string arp_pattern_str = index_str + "-arp_style";
        atom arp_pattern_value = param_dict[symbol(arp_pattern_str)];
        spring.arp.setStyle(static_cast<std::string>(arp_pattern_value));

        // Get arp_steps
        std::string arp_steps_str = index_str + "-arp_steps";
        atom arp_steps_value = param_dict[symbol(arp_steps_str)];
        spring.arp.setSteps(arp_steps_value);

        // Get arp_jump
        std::string arp_jump_str = index_str + "-arp_jump";
        atom arp_jump_value = param_dict[symbol(arp_jump_str)];
        spring.arp.setJump(arp_jump_value);

        // Get arp_octaves
        std::string arp_octaves_str = index_str + "-arp_octaves";
        atom arp_octaves_value = param_dict[symbol(arp_octaves_str)];
        spring.arp.setOctaves(arp_octaves_value);

        // Get arp_reset
        std::string arp_reset_str = index_str + "-arp_reset";
        atom arp_reset_value = param_dict[symbol(arp_reset_str)];
        spring.arp_reset = arp_reset_value;
        
        return spring;
    }

    void chordChanged()
    {
        for(auto& spring : springs)
        {
            if(spring.arp_reset == 1) spring.arp.reset();
        }
    }

    // A dictionary for the springs parameters
    dict param_dict = dict(symbol("springs-param-dict"));
    // Current Spring 
    int current_spring = 1;
    // Array of Springs
    Spring springs[16] = {Spring()}; // 16 Springs. Index 0 is not used. But could be used for copy paste? 
};

class repitch : public object<repitch> {
public:
    MIN_DESCRIPTION	{"Remap pitches on incomming midi to nearest allowed pitch."};
    MIN_TAGS		{"tromleorkestret"};
    MIN_AUTHOR		{"robotic-4udio"};
    MIN_RELATED		{"makenote, notein, noteout"};


    enum class notefication_type {
        chord_changed,
        playing_changed,
        enum_count
    };

    // Constructor
    repitch(const atoms& args = {}) {
        // Add this instance to the list of instances
        s_instances.insert(this);
        // Optionally, print the number of instances to the console
        // cout << "Number of instances: " << s_instances.size() << endl;
    }

    // Destructor
    ~repitch() {
        // Remove this instance from the list of instances
        s_instances.erase(this);

        // Optionally, print the number of instances to the console
        // cout << "Number of instances: " << s_instances.size() << endl;
    }



    // Make inlets and outlets
    inlet<>  input	{ this, "(bang) post greeting to the max console" };
    outlet<thread_check::scheduler, thread_action::fifo> out1 { this, "(anything) output the message which is posted to the max console" };
    outlet<thread_check::scheduler, thread_action::fifo> out2 { this, "(bang) when the chord changes" };

    // The playing position in the Live Set, in beats.
    message<threadsafe::yes> number { this, "number", "The playing position in the Live Set, in beats.", 
        MIN_FUNCTION {
            s_live_set.set_beats(static_cast<double>(args[0]) - offset);

            // Get the new chord
            const auto& new_chord = s_chord_track.get_chord_at_time(s_live_set.get_beats());

            // If the chord has changed
            if(new_chord != s_current_chord) {
                s_current_chord = new_chord;
                s_chord = new_chord.m_chord;
                s_pitch_vector = s_chord.getNotes().getPitch();
                notify_all(this, notefication_type::chord_changed);
            }
 
            return {};
        }  
    };

    // Is Live's transport is running?
    message<threadsafe::yes> playing { this, "playing", "Is Live's transport is running?",
        MIN_FUNCTION {
            s_live_set.set_is_playing(args[0]);
            notify_all(this, notefication_type::playing_changed);
            return {};
        }  
    };

    // Set the tempo of the Live Set
    message<threadsafe::yes> tempo { this, "tempo", "Set the tempo of the Live Set.",
        MIN_FUNCTION {
            s_live_set.set_tempo(args[0]);
            return {};
        }  
    };

    message<> getLiveClock { this, "getLiveClock", "Get the Live Clock",
        MIN_FUNCTION {
            c74::max::t_itm* clock_source = (c74::max::t_itm*) c74::max::itm_getnamed(c74::max::gensym("live"), nullptr, nullptr, 1L);

            auto itm_tic = itm_getticks(clock_source);
            double itm_tem =  itm_gettempo(clock_source);
            auto itm_res = 480.0;
            auto itm_bea = itm_tic / itm_res;

            cout << "itm_tempo: " << itm_tem << " itm_ticks: " << itm_tic << " itm_beats: " << itm_bea << endl;

            return {};
        }
    };

    // Set Chord Manually
    message<> set_chord { this, "set_chord", "Input the Chord, format: 'C7', 'C7 0 4 F7 4 12 B 12 16'",
        MIN_FUNCTION {
            
            s_chord.setChord(args[0]);
            s_pitch_vector = s_chord.getNotes().getPitch();

            // cout << s_chord.toString() << " - " << s_chord.getNotes() << endl;

            notify_all(this, notefication_type::chord_changed);

            return {};
        }
    };

    // Set ChordTrack
    message<> set_chord_track { this, "set_chord_track", "Input the ChordTrack, format: 'C7 0 4 F7 4 12 B 12 16', Chord, StartTime, EndTime",
        MIN_FUNCTION {
            s_chord_track.from_atoms(args);

            cout << s_chord_track << endl;

            number(s_beats < 0 ? 0 : static_cast<double>(s_beats + 0.0001));

            return {};
        }  
    };




    // Attribute for mode of operation. 
    // 0: The note is allowed to play until released
    // 1: The note is killed if it is not in the allowed notes
    attribute<int> mode {this, "mode", 0,
        description {"Mode of operation."},
        range {0, 1},  // Define a range for the parameter
        setter {MIN_FUNCTION {
            // Optional: add logic when the parameter value is set
            cout << "Mode set to " << args[0] << endl;
            return args;
        }}
    };


    // Attribute for note mode
    enum class note_modes : int { pass, quantize, step, enum_count };
    enum_map note_modes_range = {"pass", "quantize", "step"};
    attribute<note_modes> note_mode { this, "note_mode", note_modes::pass, note_modes_range,
        description {"Choose what happens when a note is played."}
    };

    attribute<int> play_chord {this, "play_chord", 0,
        description {"When the chord changes, the chord is played"},
        range {0, 1},  // Define a range for the parameter
        setter {MIN_FUNCTION {
            int play_chord = args[0];
            cout << "play_chord set to " << play_chord << endl;
            return args;
        }}
    };

    // pitch_min, picth_max
    attribute<int, threadsafe::yes> pitch_min {this, "pitch_min", 0,
        description {"Minimum pitch allowed."},
        range {0, 127},  // Define a range for the parameter
        setter {MIN_FUNCTION {
            return args;
        }}
    };

    attribute<int, threadsafe::yes> pitch_max {this, "pitch_max", 127,
        description {"Maximum pitch allowed."},
        range {0, 127},  // Define a range for the parameter
        setter {MIN_FUNCTION {
            return args;
        }}
    };


    // Attribute to offset the beats time from the Live Set
    attribute<double> offset {this, "offset", 0.0,
        description {"Offset the beats time from the Live Set."},
        range {-1.0, 1.0},  // Define a range for the parameter
        setter {MIN_FUNCTION {
            return args;
        }}
    };


    // Notify all instances of a change
    static void notify_all(repitch* notifying_instance, notefication_type type) {
        for (auto instance : s_instances) instance->notify(notifying_instance, type);
    }

    // Notify this instance of a change. Pass the type of change as an argument and also the instance that is notifying.
    void notify(repitch* notifying_instance, notefication_type type) {
        switch (type) {
            case notefication_type::chord_changed: 
                chord_changed(notifying_instance);
            break;
            case notefication_type::playing_changed:
                playing_changed(notifying_instance);
            break;
            case notefication_type::enum_count:
            break;
        }
    }

    // Notification function for chord_changed
    void chord_changed(repitch* notifying_instance)
    {
        if(play_chord == 1) {
            playChord(0);
            if(s_live_set.get_is_playing()){
                playChord(100);
            }
        }

        // Reset all the spring arps
        m_springs.chordChanged();

        // Send a bang to the second outlet to notify that the chord has changed
        out2.send("bang");
    }

    // Notification function for playing_changed
    void playing_changed(repitch* notifying_instance)
    {
        if(s_live_set.get_is_playing() && play_chord == 1){
            playChord(100);
        }
        else {
            playChord(0);
        }
    }

    // playChord
    void playChord(int velocity) {
        // Note Off
        if(velocity==0)
        {
            noteOff(-1);
            return;
        }

        // Note On
        for(auto pitch : s_pitch_vector){
            pitch = pitchToRange(pitch);
            noteOn(-1, pitch, velocity);
        }
    }

    // Get the chord as a string
    message<threadsafe::yes> get_chord {this, "get_chord", "Return the chord symbol.",
        MIN_FUNCTION {
            atoms res;
            res.push_back("chord");
            res.push_back(s_chord.toString());

            out1.send(res);

            return {};
        } 
    };

    // Get the pitch vector from the chord
    message<threadsafe::yes> get_pitch_vector {this, "get_pitch_vector", "Return the midinotes of the chord tones.",
        MIN_FUNCTION {
            atoms res;
            res.push_back("pitch_vector");
            
            if(args.size() == 1) {
                for(auto pitch : s_pitch_vector){
                    int octave = args[0];
                    int transpose =  (octave * 12 + cmtk::C0) - cmtk::C3;
                    pitch = pitchToRange(pitch + transpose);
                    res.push_back(pitch);
                }
            }
            else if(args.size() == 2 && static_cast<int>(args[1]) - static_cast<int>(args[0]) >= 11) {
                int low = args[0];
                int high = args[1];
                for(auto pitch : s_pitch_vector){
                    pitch = pitchToRange(pitch, low, high);
                    res.push_back(pitch);
                }
            }
            else {            
                for(auto pitch : s_pitch_vector) {
                    pitch = pitchToRange(pitch);
                    res.push_back(pitch);
                }
            }

            out1.send(res);

            return {};
        } 
    };

    // Get the Root of the chord
    message<threadsafe::yes> get_root {this, "get_root", "Return the root of the chord.",
        MIN_FUNCTION {
            int pitch = -1;

            if( (args.size() == 2) && (static_cast<int>(args[1])-static_cast<int>(args[0]) >= 11) ) {
                int low = args[0];
                int high = args[1];
                pitch = s_chord.getRoot(low,high).getPitch();
            }
            else {
                pitch = s_chord.getRoot().getPitch();
            }

            pitch = pitchToRange(pitch);
            atom res = pitch;
            out1.send(res);

            return {};
        } 
    };

    // Get the Bass of the chord
    message<threadsafe::yes> get_bass {this, "get_bass", "Return the bass of the chord.",
        MIN_FUNCTION {
            int pitch = -1;

            if(args.size() == 2 && static_cast<int>(args[1]) - static_cast<int>(args[0]) >= 11) {
                int low = args[0];
                int high = args[1];
                pitch = s_chord.getBass(low,high).getPitch();
            }
            else {
                pitch = s_chord.getBass().getPitch();
            }

            pitch = pitchToRange(pitch);
            atom res = pitch;
            out1.send(res);

            return {};
        } 
    };

    // Get the Random pitch from the chord
    message<threadsafe::yes> get_rand {this, "get_rand", "Return a random pitch from the chord.",
        MIN_FUNCTION {
            int pitch = -1;
            atom res;

            if(args.size() == 2 && static_cast<int>(args[1]) - static_cast<int>(args[0]) >= 11) {
                int low = args[0];
                int high = args[1];
                pitch = s_chord.getRandNote(low, high).getPitch();
            }
            else {
                pitch = s_chord.getRandNote().getPitch();
            }

            res = pitchToRange(pitch);
            out1.send(res);
            
            return {};
        } 
    };

    int pitchToRange(int pitch){
        while(pitch < pitch_min) pitch += 12;
        while(pitch > pitch_max) pitch -= 12;
        return pitch;
    }

    int pitchToRange(int pitch, int low, int high) const 
    {
        // Respect the attributes
        if(pitch_min < low)  low  = pitch_min;
        if(pitch_max > high) high = pitch_max;
        while(pitch < low)  pitch += 12;
        while(pitch > high) pitch -= 12;
        return pitch;
    }

    // Get Pitch At
    message<threadsafe::yes> noteAt {this, "noteAt", "Return the pitch at the given index.",
        MIN_FUNCTION {

            int pitch = -1;
            int low = pitch_min;
            int high = pitch_max;

            if(args.empty()) {
                cerr << "noteAt message requires at least one argument: index." << endl;
                return {};
            }

            switch(args[0].type())
            {
                case message_type::int_argument:
                case message_type::float_argument:
                {
                    int index = args[0];
                    pitch = s_chord.getNoteAt(index).getPitch();
                    pitch = pitchToRange(pitch, low, high);
                }
                break;
                case message_type::symbol_argument:{
                    std::string str = args[0];
                    if     (str == "root") pitch = s_chord.getRoot().getPitch();
                    else if(str == "bass") pitch = s_chord.getBass().getPitch();
                    else if(str == "high") pitch = s_chord.getNotes().back().getPitch();
                    else if(str == "low" ) pitch = s_chord.getNotes().front().getPitch();
                    else if(str == "rand") pitch = s_chord.getRandNote(pitch_min, pitch_max).getPitch();
                    else {
                        cerr << "noteAt message requires a valid index: index, 'r (root)' or '(b) bass'." << endl;
                        return {};
                    }
                }
                break;
                default:
                    cerr << "noteAt message requires a valid index: index, 'r (root)' or '(b) bass'." << endl;
                    return {};
            }


            atom res = pitchToRange(pitch);
            out1.send(res);
            return {};

        } 
    };

    // Get Quantized to Chord Pitch (accepts lists)
    message<threadsafe::yes> quantize {this, "quantize", "Quantize the pitch to the nearest chord pitch.",
        MIN_FUNCTION {
            // Check that we get one argument for pitch
            if (args.size() < 1) {
                cerr << "quantize message requires one argument: pitch." << endl;
                return {};
            }

            atoms res;
            res.push_back("quantized");
            for (const auto& arg : args) {
                int pitch_in = arg;
                int pitch_out = find_nearest_pitch(pitch_in);
                pitch_out = pitchToRange(pitch_out);
                res.push_back(pitch_out);
            }

            out1.send(res);

            return {};
        } 
    };

    // Note message: The note will be repitched to nearest chord pitch
    message<threadsafe::yes> note {this, "note", "Midi note message. If not in the allowed notes, the note is repitched.",
        MIN_FUNCTION {
            // Check that we get two arguments for pitch and velocity
            if (args.size() != 2) {
                cout << "Error: note message requires two arguments: pitch and velocity." << endl;
                return {};
            }

            int pitch_in = args[0];
            int velocity = args[1];

            // Check that the velocity is between 0 and 127
            if(velocity < 0 || velocity > 127) {
                cout << "Error: velocity must be between 0 and 127." << endl;
                return {};
            }

            if(velocity == 0) 
            {
                noteOff(pitch_in);
            }
            else 
            {
                int pitch_out = pitch_in;

                switch(note_mode) {
                    case note_modes::pass:{ 
                    }
                    break;
                    case note_modes::quantize:{ 
                        pitch_out = find_nearest_pitch(pitch_in); 
                    } 
                    break; 
                    case note_modes::step:{ 
                        auto index = pitch_in-60;
                        pitch_out = s_chord.getNoteAt(index).getPitch();
                    } 
                    break;
                    case note_modes::enum_count: 
                    break;
                }

                // Make Note On
                noteOn(pitch_in, pitch_out, velocity);
            }

            return {};
        }
    };

    void noteOff(int pitch_in)
    {
        // Check if a note with the same pitch_in is already playing, if so, send noteoff and remove the note from the set
        auto it = std::remove_if(playing_notes.begin(), playing_notes.end(), [pitch_in, this](const Note& note) {
            const bool should_remove = note.m_pitch_in == pitch_in;
            if(should_remove) this->out1.send("note", note.m_pitch_out, 0);
            return should_remove;
        });
        playing_notes.erase(it, playing_notes.end());
    }

    void noteOn(int pitch_in, int pitch_out, int velocity)
    {
        // Get the pitch in range
        pitch_out = pitchToRange(pitch_out);

        // Check if a note with the same pitch_in is already playing, if so, send noteoff and remove the note from the set
        auto it = std::remove_if(playing_notes.begin(), playing_notes.end(), [pitch_in, pitch_out, this](const Note& note) {
            const bool should_remove = (note.m_pitch_out == pitch_out) || (note.m_pitch_in >= 0 && note.m_pitch_in == pitch_in);
            if(should_remove) this->out1.send("note", note.m_pitch_out, 0);
            return should_remove;
        });
        playing_notes.erase(it, playing_notes.end());
        
        // Create a new note
        Note new_note = Note(pitch_in, pitch_out, velocity);

        // Send a noteon message
        out1.send("note", new_note.m_pitch_out, new_note.m_velocity);

        // Add the note to the set of playing notes
        playing_notes.push_back(new_note);
    }

    // Get value from the dictionary
    message<threadsafe::yes> get_springs_param {this, "get_springs_param", "Get a value from the dictionary.",
        MIN_FUNCTION {
            if(args.size() != 1) {
                cerr << "Error: get message requires one argument: key." << endl;
                return {};
            }

            atoms res = m_springs.param_dict[symbol(args[0])];
            out1.send(res);

            return {};
        }
    };

    // Get value from the dictionary
    message<threadsafe::yes> get_spring_params {this, "get_spring_params", "Output the parameters for the selected spring.",
        MIN_FUNCTION {
            if(args.size() != 1) {
                cerr << "Error: get message requires one argument: key." << endl;
                return {};
            }

            const auto& spring = m_springs.getSpring(args[0]);
            out1.send("springs", "param", "midinote"           , spring.midinote);
            out1.send("springs", "param", "inst"               , spring.inst);
            out1.send("springs", "param", "mode"               , spring.mode);
            out1.send("springs", "param", "note"               , spring.note);
            out1.send("springs", "param", "transpose"          , spring.transpose);
            out1.send("springs", "param", "pitch_min"          , spring.pitch_min);
            out1.send("springs", "param", "pitch_max"          , spring.pitch_max);
            out1.send("springs", "param", "octave_add"         , spring.octave_add);
            out1.send("springs", "param", "velocity"           , spring.velocity);
            out1.send("springs", "param", "velocity_deviation" , spring.velocity_deviation);
            out1.send("springs", "param", "duration"           , spring.duration);
            out1.send("springs", "param", "arp_style"          , spring.arp.getStyle());
            out1.send("springs", "param", "arp_steps"          , spring.arp.getSteps());
            out1.send("springs", "param", "arp_jump"           , spring.arp.getJump());
            out1.send("springs", "param", "arp_octaves"        , spring.arp.getOctaves());
            out1.send("springs", "param", "arp_reset"     , spring.arp_reset);


            return {};
        }
    };

    // Set spring parameters, args[0] is the index, args[1] is the key, args[2] is the value
    message<threadsafe::yes> set_springs_param {this, "set_springs_param", "The arguments are index, parameter, value",
        MIN_FUNCTION {
            if(args.size() != 3) {
                cerr << "Error: set message requires three arguments: key and value." << endl;
                return {};
            }

            std::string index = args[0];
            std::string param = args[1];
            std::string key = index + "-" + param;

            // Add the rest of the arguments to value, skipping the first 2 arguments
            switch(args[2].type())
            {
                case message_type::int_argument:
                case message_type::float_argument:
                {
                    int value = args[2];
                    m_springs.param_dict[symbol(key)] = value;
                }
                break;
                case message_type::symbol_argument:{
                    symbol value = args[2];
                    m_springs.param_dict[symbol(key)] = value;
                }
                break;
                default:
                    cerr << "set_springs_param message requires a valid index: index, 'r (root)' or '(b) bass'." << endl;
                    return {};
            }
            
            return {};
        }
    };

    // Select Spring
    message<threadsafe::yes> select_spring {this, "select_spring", "Select the spring to edit.",
        MIN_FUNCTION {
            if(args.size() != 1) {
                cerr << "Error: select_spring message requires one argument: index." << endl;
                return {};
            }

            m_springs.current_spring = args[0];

            get_spring_params(m_springs.current_spring);

            return {};
        }
    };

    // Set spring parameters, args[1] is the key, args[2] is the value
    message<threadsafe::yes> set_spring_param {this, "set_spring_param", "Set a parameter for the selected spring. Args: parameter, value",
        MIN_FUNCTION {
            if(args.size() != 2) {
                cerr << "Error: set message requires two arguments: key and value." << endl;
                return {};
            }

            atoms myArgs;
            // Spring Index
            myArgs.push_back(m_springs.current_spring);
            // Add the rest of the arguments
            for(auto& arg : args) myArgs.push_back(arg);
            // Call the set_springs_param function
            set_springs_param(myArgs);
            
            return {};
        }
    };

    void outSpringNote(const Springs::Spring& spring, int pitch)
    {
        out1.send("springs", "makenote", pitch, spring.getVelocity(), spring.duration, spring.inst);
    };

    // SpringTrig
    message<threadsafe::yes> springTrig {this, "springTrig", "Trigger the spring at index.",
        MIN_FUNCTION {
            if(args.size() != 1) {
                cerr << "Error: springTrig message requires one argument: index." << endl;
                return {};
            }

            auto& spring = m_springs.getSpring(args[0]);
            std::vector<int> pitch;

            // Find the pitch according to the mode
            if     (spring.mode == "midinote") pitch.push_back(spring.note);
            else if(spring.mode == "quantize") pitch.push_back(find_nearest_pitch(spring.note)); 
            else if(spring.mode == "step")     pitch.push_back(s_chord.getNoteAt(spring.note-60).getPitch());
            else if(spring.mode == "root")     pitch.push_back(s_chord.getRoot().getPitch());
            else if(spring.mode == "bass")     pitch.push_back(s_chord.getBass().getPitch());
            else if(spring.mode == "high")     pitch.push_back(s_chord.getNotes().back().getPitch());
            else if(spring.mode == "low" )     pitch.push_back(s_chord.getNotes().front().getPitch());
            else if(spring.mode == "rand")     pitch.push_back(s_chord.getRandNote(spring.pitch_min, spring.pitch_max).getPitch());
            else if(spring.mode == "arp")      pitch.push_back(spring.arp.next(s_chord));
            else if(spring.mode == "chord")    pitch = s_chord.getNotes().getPitch();
            else {
                cerr << "Error: Unknown mode: " << spring.mode << endl;
                return {};
            }

            // Make the notes
            for(auto p : pitch) {
                // Transpose the pitch
                p += spring.transpose;

                // Get the pitch in range
                while(p < spring.pitch_min) p += 12;
                while(p > spring.pitch_max) p -= 12;

                // MakeNote
                outSpringNote(spring, p);

                // If octave_add is set, make a new note
                if(spring.octave_add != 0) {
                    p += spring.octave_add * 12;
                    outSpringNote(spring, p);
                }
            }

            return {};
        }
    };



private:

    // Check if a pitch is in the chord
    bool is_pitch_in_chord(int pitch){
        int p = pitch;

        // If p is in picth_vector, return true
        if(cmtk::contains(s_pitch_vector,p)) return true;

        // Check octaves below
        p = pitch - 12;
        while(p >= 0)
        {
            if(cmtk::contains(s_pitch_vector,p)) return true;
            p -= 12;
        }

        // Check octaves above
        p = pitch + 12;
        while(p < 128)
        {
            if(cmtk::contains(s_pitch_vector,p)) return true;
            p += 12;
        }
        
        // If no allowed pitch is found, return false
        return false;
    }

    // Find the nearest pitch in the chord
   int find_nearest_pitch(int pitch){
        
        if(is_pitch_in_chord(pitch)) return pitch;

        // Find the nearest pitch
        for(int i=1; i<7; i++) {
            if(is_pitch_in_chord(pitch - i)){
                return pitch - i;
            }
            if(is_pitch_in_chord(pitch + i)){
                return pitch + i;
            }
        }
        
        // If no pitch is found, post an error and return the pitch
        cerr << "Error: No allowed pitch found for pitch " << pitch << endl;
        return pitch;
    }
    
    // List of all playing notes
    std::vector<Note> playing_notes = {};

    // Static list of all instances of the class
    static std::set<repitch*> s_instances;
    
    // Static ChordTrack
    static ChordTrack s_chord_track;
    static ChordTrack::TimedChord s_current_chord;
    static cmtk::Chord s_chord;
    static std::vector<int> s_pitch_vector;



    // Static Beat
    static double s_beats;

    // A structure to hold information about the live set
    static LiveSet s_live_set;

    // Instrumets
    Springs m_springs = Springs();


};


// The implementation of the constructor for the note class ...
// The order of initialization is critical
Note::Note(int a_pitch_in, int a_pitch_out, int a_velocity)
    : m_pitch_in {a_pitch_in}
    , m_pitch_out {a_pitch_out}
    , m_velocity {a_velocity}
    , m_id {++s_counter}
{
}


// Init Static variables
std::set<repitch*> repitch::s_instances = {};

long Note::s_counter = 0;

// Init ChordTrack
ChordTrack repitch::s_chord_track = ChordTrack();
ChordTrack::TimedChord repitch::s_current_chord = ChordTrack::TimedChord();
cmtk::Chord repitch::s_chord = cmtk::Chord("Fm");
std::vector<int> repitch::s_pitch_vector = {};


// Init Static Beat
double repitch::s_beats = -1.0;
LiveSet repitch::s_live_set = LiveSet();


MIN_EXTERNAL(repitch);
