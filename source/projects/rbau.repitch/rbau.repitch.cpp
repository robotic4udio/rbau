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

using namespace c74::min;

// A class to represent a note. A note that can be repitched, therefore we store both pitch_in and pitch_out.


class ChordTrack {
public:
    struct Chord {
    public:
        Chord() : m_chord(""), m_start(-1), m_end(-1) {}
        Chord(std::string a_chord, double a_start, double a_end) : m_chord(a_chord), m_start(a_start), m_end(a_end) {}
        std::string m_chord;
        double      m_start;
        double      m_end;

        // Stream operator to print the chord
        friend std::ostream& operator<<(std::ostream& os, const Chord& chord) {
            os << "Chord:(" << chord.m_chord << "," << chord.m_start << "," << chord.m_end << ")";
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
        m_chords.push_back(Chord(a_chord, a_start, a_end));
    }

    // Get Chord at time
    const Chord& get_chord_at_time(double time) const {
        for(auto it = m_chords.rbegin(); it != m_chords.rend(); ++it) {
            if(it->m_start <= time) return *it;
        }
        // Return an empty chord if no chord is found
        return Chord("", 0, 0);
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
    std::vector<Chord> m_chords;

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

class repitch : public object<repitch> {
public:
    MIN_DESCRIPTION	{"Remap pitches on incomming midi to nearest allowed pitch."};
    MIN_TAGS		{"tromleorkestret"};
    MIN_AUTHOR		{"robotic-4udio"};
    MIN_RELATED		{"makenote, notein, noteout"};


    enum class notefication_type {
        chord_changed
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
    outlet<thread_check::scheduler, thread_action::fifo> out1	{ this, "(anything) output the message which is posted to the max console" };
    outlet<thread_check::scheduler, thread_action::fifo> out2 { this, "(bang) when the chord changes" };



    // Transport Time in Ticks for the chord track
    message<threadsafe::yes> number { this, "number", "Transport Time in Ticks", 
        MIN_FUNCTION {
            // Post the number of instances to the console
            // cout << "Transport Time in Beats: " << static_cast<double>(args[0]) * s_beats_per_tick << endl;
            double last_beats = s_beats;
            s_beats = static_cast<double>(args[0]) * s_beats_per_tick;

            // Don't do anything if the beats are the same
            if(last_beats == s_beats) return {};
            last_beats = s_beats;

            // Get the new chord
            const auto& new_chord = s_chord_track.get_chord_at_time(s_beats);

            if(new_chord.m_chord != s_current_chord.m_chord) {
                s_current_chord = new_chord;
                set_chord(new_chord.m_chord);
            }
 
            return {};
        }  
    };

    // Set Chord Manually
    message<> set_chord { this, "set_chord", "Input the Chord, format: 'C7', 'C7 0 4 F7 4 12 B 12 16'",
        MIN_FUNCTION {
            
            s_chord.setChord(args[0]);
            s_pitch_vector = s_chord.getNotes().getPitch();

            cout << s_chord.toString() << " - " << s_chord.getNotes() << endl;

            notify_all(this, notefication_type::chord_changed);

            return {};
        }  
    };

    // Set ChordTrack
    message<> set_chord_track { this, "set_chord_track", "Input the ChordTrack, format: 'C7 0 4 F7 4 12 B 12 16', Chord, StartTime, EndTime",
        MIN_FUNCTION {
            // Post the number of instances to the console
            // cout << "Transport Time in Beats: " << static_cast<double>(args[0]) * s_beats_per_tick << endl;
            s_chord_track.from_atoms(args);

            cout << s_chord_track << endl;

            number(s_beats < 0 ? 0 : static_cast<double>(s_beats / s_beats_per_tick + 0.0001));

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

    // Notify all instances of a change
    static void notify_all(repitch* notifying_instance, notefication_type type) {
        for (auto instance : s_instances){
            instance->notify(notifying_instance, type);
        }
    }

    // Notify this instance of a change. Pass the type of change as an argument and also the instance that is notifying.
    void notify(repitch* notifying_instance, notefication_type type) {
        switch (type) {
            case notefication_type::chord_changed: 
                chord_changed(notifying_instance);
            break;
        }
    }

    // Notifycation function for allowed notes changed
    void chord_changed(repitch* notifying_instance)
    {
        out2.send("bang");
    }

    // Get the chord as a string
    message<threadsafe::yes> get_chord {this, "get_chord", "Return the chord.",
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
                    res.push_back(pitch+transpose);
                }
            }
            else if(args.size() == 2 && static_cast<int>(args[1]) - static_cast<int>(args[0]) >= 11) {
                int low = args[0];
                int high = args[1];
                for(auto pitch : s_pitch_vector){
                    while(pitch < low) pitch += 12;
                    while(pitch > high) pitch -= 12;
                    res.push_back(pitch);
                }
            }
            else {            
                for(auto pitch : s_pitch_vector) {
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
            atoms res;
            res.push_back("root");

            if( (args.size() == 2) && (static_cast<int>(args[1])-static_cast<int>(args[0]) >= 11) ) {
                int low = args[0];
                int high = args[1];
                res.push_back(s_chord.getRoot(low,high).getPitch());
            }
            else {
                res.push_back(s_chord.getRoot().getPitch());
            }

            out1.send(res);

            return {};
        } 
    };

    // Get the Bass of the chord
    message<threadsafe::yes> get_bass {this, "get_bass", "Return the bass of the chord.",
        MIN_FUNCTION {

            atoms res;
            res.push_back("bass");

            if(args.size() == 2 && static_cast<int>(args[1]) - static_cast<int>(args[0]) >= 11) {
                int low = args[0];
                int high = args[1];
                res.push_back(s_chord.getBass(low,high).getPitch());
            }
            else {
                res.push_back(s_chord.getBass().getPitch());
            }


            out1.send(res);

            return {};
        } 
    };

    // Get the Random note from the chord
    message<threadsafe::yes> get_rand_note {this, "get_rand_note", "Return a random note from the chord.",
        MIN_FUNCTION {

            atoms res;
            res.push_back("rand_note");

            // Get a random note from the pitch vector
            int note = s_pitch_vector[rand() % s_pitch_vector.size()];

            if(args.size() == 2 && static_cast<int>(args[1]) - static_cast<int>(args[0]) >= 11) {
                int low = args[0];
                int high = args[1];

                int rand_octave = -10 + rand() % 20;

                note += (rand_octave * 12);

                // Make sure the note is within the range
                while(note < low)  note += 12;
                while(note > high) note -= 12;
                res.push_back(note);
            }
            else {
                res.push_back(note);
            }


            out1.send(res);

            return {};
        } 
    };


    // Get Quantized to Chord Note (accepts lists)
    message<threadsafe::yes> quantize {this, "quantize", "Quantize the pitch to the nearest chord note.",
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
                res.push_back(pitch_out);
            }

            out1.send(res);

            return {};
        } 
    };




    // Note message: The note will be repitched to nearest chord note
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
                // Check if a note with the same pitch_in is already playing, if so, send noteoff and remove the note from the set
                auto it = std::remove_if(playing_notes.begin(), playing_notes.end(), [pitch_in, this](const Note& note) {
                    const bool should_remove = note.m_pitch_in == pitch_in;
                    if(should_remove) this->out1.send("note", note.m_pitch_out, 0);
                    return should_remove;
                });
                playing_notes.erase(it, playing_notes.end());
            }
            else 
            {
                int pitch_out = find_nearest_pitch(pitch_in);

                // Check if a note with the same pitch_in is already playing, if so, send noteoff and remove the note from the set
                auto it = std::remove_if(playing_notes.begin(), playing_notes.end(), [pitch_in, pitch_out, this](const Note& note) {
                    const bool should_remove = (note.m_pitch_in == pitch_in) || (note.m_pitch_out == pitch_out);
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
    static ChordTrack::Chord s_current_chord;
    static cmtk::Chord s_chord;
    static std::vector<int> s_pitch_vector;

    // Static Beat
    static double s_beats_per_tick;
    static double s_beats;

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
ChordTrack::Chord repitch::s_current_chord = ChordTrack::Chord();
cmtk::Chord repitch::s_chord = cmtk::Chord();
std::vector<int> repitch::s_pitch_vector = {};

// Init Static Beat
double repitch::s_beats_per_tick = 1.0/480.0;
double repitch::s_beats = -1.0;


MIN_EXTERNAL(repitch);
