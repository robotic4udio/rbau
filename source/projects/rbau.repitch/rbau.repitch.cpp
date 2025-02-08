/// @file
///	@ingroup 	robotic-audio 
///	@copyright	Copyright 2025 Hjalte Bested Hjorth. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <set>

using namespace c74::min;

// A class to represent a note. A note that can be repitched, therefore we store both pitch_in and pitch_out.
class Note {
public:
    Note(int a_pitch_in, int a_pitch_out, int a_velocity);

    long id() {
        return m_id;
    }

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
        allowed_notes_changed
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
    outlet<thread_check::scheduler, thread_action::fifo> output	{ this, "(anything) output the message which is posted to the max console" };


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
            case notefication_type::allowed_notes_changed: 
                allowed_notes_changed(notifying_instance);
                break;
        }
    }

    // Notifycation function for allowed notes changed
    void allowed_notes_changed(repitch* notifying_instance)
    {
        // Optionally, print the allowed notes to the console for verification
        // cout << "Allowed notes changed by instance " << notifying_instance << endl;

        output_allowed_notes();
    }

    // Message allowed_notes:
    message<threadsafe::yes> set_allowed_notes {this, "set_allowed_notes", "Input a vector of allowed notes.",
        MIN_FUNCTION {

            // Clear the current set of allowed notes
            for(int i = 0; i < 128; i++){
                s_playing_allowed_notes[i] = false;
            }

            // Fill the set of pitches
            for (auto arg : args) {
                if (arg.a_type == c74::max::A_LONG || arg.a_type == c74::max::A_FLOAT){
                    int pitch = arg;
                    s_playing_allowed_notes[pitch] = true;
                }
            }

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        } 
    };

    // Add allowed notes to the set
    message<threadsafe::yes> add_allowed_notes {this, "add_allowed_notes", "Input a vector of allowed notes to add.",
        MIN_FUNCTION {
            // Fill the set of pitches - one octave
            for (auto arg : args) {
                if (arg.a_type == c74::max::A_LONG || arg.a_type == c74::max::A_FLOAT){
                    int pitch = arg;
                    s_playing_allowed_notes[pitch] = true;
                }
            }

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        } 
    };

    // Remove allowed notes from the set
    message<threadsafe::yes> remove_allowed_notes {this, "remove_allowed_notes", "Input a vector of allowed notes to remove.",
        MIN_FUNCTION {
            // Fill the set of pitches - one octave
            for (auto arg : args) {
                if (arg.a_type == c74::max::A_LONG || arg.a_type == c74::max::A_FLOAT){
                    int pitch = arg;
                    s_playing_allowed_notes[pitch] = false;
                }
            }

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        } 
    };


    // get_allowed_notes:
    message<threadsafe::yes> get_allowed_notes {this, "get_allowed_notes", "Output the allowed notes.",
        MIN_FUNCTION {

            output_allowed_notes();

            return {};
        } 
    };


    void output_allowed_notes(){
        atoms allowed_notes;
        allowed_notes.push_back("allowed_notes");
        
        for(int i = 0; i < 128; i++) {
            if(s_playing_allowed_notes[i]) {
                allowed_notes.push_back(i);
            }
        }

        output.send(allowed_notes);
    }

    // note message
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
                    if(should_remove) this->output.send("note", note.m_pitch_out, 0);
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
                    if(should_remove) this->output.send("note", note.m_pitch_out, 0);
                    return should_remove;
                });
                playing_notes.erase(it, playing_notes.end());
                
                // Create a new note
                Note new_note = Note(pitch_in, pitch_out, velocity);

                // Send a noteon message
                output.send("note", new_note.m_pitch_out, new_note.m_velocity);

                // Add the note to the set of playing notes
                playing_notes.push_back(new_note);
            }

            return {};
        }
    };

    // Allowed Note
    message<threadsafe::yes> allowed_note {this, "allowed_note", "Midi note message. If note-on it is added to the allowed notes, else it is removed.",
        MIN_FUNCTION {
            // Check that we get two arguments for pitch and velocity
            if (args.size() != 2) {
                cerr << "allowed_note message requires two arguments: pitch and velocity." << endl;
                return {};
            }

            int pitch_in = args[0];
            int velocity = args[1];


            // Check that the velocity is between 0 and 127
            if(velocity < 0 || velocity > 127) {
                cerr << "velocity must be between 0 and 127." << endl;
                return {};
            }

            // Set the allowed note
            s_playing_allowed_notes[pitch_in] = velocity;

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        }

    };  



private:

    bool is_pitch_allowed(int pitch) {        
        int p = pitch;

        // Special case
        if(s_playing_allowed_notes[p]) return true;

        // Check octaves below
        p = pitch - 12;
        while(p >= 0)
        {
            if(s_playing_allowed_notes[p]) return true;
            p -= 12;
        }

        // Check octaves above
        p = pitch + 12;
        while(p < 128)
        {
            if(s_playing_allowed_notes[p]) return true;
            p += 12;
        }
        
        // If no allowed pitch is found, return false
        return false;
    }


   int find_nearest_pitch(int pitch){
        
        if(is_pitch_allowed(pitch)) return pitch;

        // Find the nearest pitch
        for(int i=1; i<7; i++) {
            if(is_pitch_allowed(pitch - i)){
                return pitch - i;
            }
            else if(is_pitch_allowed(pitch + i)){
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

    // Static list of all allowed pitches
    static bool s_playing_allowed_notes[128];

    // Mutex for thread safety
    mutex m_mutex;


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


// Init s_playing_allowed_notes to false
bool repitch::s_playing_allowed_notes[128] = {false};

// Init Static variables
std::set<repitch*> repitch::s_instances = {};

long Note::s_counter = 0;





MIN_EXTERNAL(repitch);
