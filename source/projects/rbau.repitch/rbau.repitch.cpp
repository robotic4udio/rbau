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
        return m_pitch_in < other.m_pitch_in;
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
        cout << "Number of instances: " << s_instances.size() << endl;
    }

    // Destructor
    ~repitch() {
        // Remove this instance from the list of instances
        s_instances.erase(this);

        // Optionally, print the number of instances to the console
        cout << "Number of instances: " << s_instances.size() << endl;
    }

    // Make inlets and outlets
    inlet<>  input	{ this, "(bang) post greeting to the max console" };
    outlet<> output	{ this, "(anything) output the message which is posted to the max console" };


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
        for (auto instance : s_instances) {
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
    void allowed_notes_changed(repitch* notifying_instance) {

        // 1: The note is killed if it is not in the allowed notes
        if(mode == 1)
        {
            // Use remove_if to get an iterator to the first element that should be removed
            auto it = std::remove_if(s_playing_notes.begin(), s_playing_notes.end(), [this](const Note& note) {
            return s_allowed_pitches_full_set.find(note.m_pitch_out) == s_allowed_pitches_full_set.end();
            });

            // Send noteoff for all playing notes that are not in the allowed notes by using the iterator returned by remove_if
            for (auto note = it; note != s_playing_notes.end(); note++) {
                output.send("note", note->m_pitch_out, 0);
            }

            // Remove the notes that are not in the allowed notes by passing the iterator returned by remove_if to erase
            s_playing_notes.erase(it, s_playing_notes.end());
        }

    }

    // Message allowed_notes:
    message<> set_allowed_notes {this, "set_allowed_notes", "Input a vector of allowed notes.",
        MIN_FUNCTION {
            s_allowed_pitches_set.clear();

            // Fill the set of pitches - one octave
            for (auto arg : args) {
                if (arg.a_type == c74::max::A_LONG){
                    int val = arg;
                    val = val % 12;
                    s_allowed_pitches_set.insert(val);
                }
            }

            // Full set of allowed pitches all octaves
            s_allowed_pitches_full_set.clear();
            for (int i = 0; i < 11; i++) {
                for (auto note : s_allowed_pitches_set) {
                    note = note + 12 * i;
                    if(note > 127) break;
                    s_allowed_pitches_full_set.insert(note);
                }
            }

            // Optionally, print the allowed notes to the console for verification
            cout << "Allowed notes: ";
            for (auto note : s_allowed_pitches_set) {
                cout << note << " ";
            }
            cout << endl;

            // Optionally, print the allowed notes to the console for verification
            cout << "All allowed notes: ";
            for (auto note : s_allowed_pitches_full_set) {
                cout << note << " ";
            }
            cout << endl;

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        } 
    };

    // Add allowed notes to the set
    message<> add_allowed_notes {this, "add_allowed_notes", "Input a vector of allowed notes to add.",
        MIN_FUNCTION {
            // Fill the set of pitches - one octave
            for (auto arg : args) {
                if (arg.a_type == c74::max::A_LONG){
                    int val = arg;
                    auto note = val % 12;
                    s_allowed_pitches_set.insert(note);
                    for (int i = 0; i < 11; i++) {
                        note = note + 12 * i;
                        if(note > 127) break;
                        s_allowed_pitches_full_set.insert(note);
                    }
                }
            }

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        } 
    };

    // Remove allowed notes from the set
    message<> remove_allowed_notes {this, "remove_allowed_notes", "Input a vector of allowed notes to remove.",
        MIN_FUNCTION {
            // Fill the set of pitches - one octave
            for (auto arg : args) {
                if (arg.a_type == c74::max::A_LONG){
                    int val = arg;
                    auto note = val % 12;
                    s_allowed_pitches_set.erase(note);
                    for (int i = 0; i < 11; i++) {
                        note = note + 12 * i;
                        if(note > 127) break;
                        s_allowed_pitches_full_set.erase(note);
                    }
                }
            }

            // Notify all instances of the change
            notify_all(this, notefication_type::allowed_notes_changed);

            return {};
        } 
    };

    // get_allowed_notes:
    message<> get_allowed_notes {this, "get_allowed_notes", "Output the allowed notes.",
        MIN_FUNCTION {
            // Optionally, print the allowed notes to the console for verification
            // cout << "Allowed notes: ";
            // for (auto note : s_allowed_pitches_set) {
            //     cout << note << " ";
            // }
            // cout << endl;

            // Send the allowed notes to the output

            output_allowed_notes();

            return {};
        } 
    };


    void output_allowed_notes() {
        atoms allowed_notes;
        allowed_notes.push_back("allowed_notes");
        
        for (auto note : s_allowed_pitches_set) {
            allowed_notes.push_back(note);
        }

        output.send(allowed_notes);
    }

    // note message
    message<> note {this, "note", "Midi note message. If not in the allowed notes, the note is repitched.",
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
                auto it = std::find_if(s_playing_notes.begin(), s_playing_notes.end(), [pitch_in](const Note& note) {
                    return note.m_pitch_in == pitch_in;
                });
                if (it != s_playing_notes.end()) {
                    output.send("note", it->m_pitch_out, 0);
                    s_playing_notes.erase(it);
                }
            }
            else 
            {
                int pitch_out = quantize_note(pitch_in);

                // Check if a note with the same pitch_in is already playing, if so, send noteoff and remove the note from the set
                auto it = std::find_if(s_playing_notes.begin(), s_playing_notes.end(), [pitch_in, pitch_out](const Note& note) {
                    return (note.m_pitch_in == pitch_in) || (note.m_pitch_out == pitch_out);
                });
                if (it != s_playing_notes.end()) {
                    output.send("note", it->m_pitch_out, 0);
                    s_playing_notes.erase(it);
                }
                
                // Create a new note
                Note new_note = Note(pitch_in, pitch_out, velocity);

                // Send a noteon message
                output.send("note", new_note.m_pitch_out, new_note.m_velocity);

                // Add the note to the set of playing notes
                s_playing_notes.push_back(new_note);
            }

            return {};
        }
    };

    // Allowed Note
    message<> allowed_note {this, "allowed_note", "Midi note message. If note-on it is added to the allowed notes, else it is removed.",
        MIN_FUNCTION {
            // Check that we get two arguments for pitch and velocity
            if (args.size() != 2) {
                cout << "Error: allowed_note message requires two arguments: pitch and velocity." << endl;
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
                remove_allowed_notes(pitch_in);

            }
            else 
            {
                add_allowed_notes(pitch_in);
            }

            // Send the allowed notes to the output
            output_allowed_notes();

            return {};
        }

    };  



private:

   int find_nearest_pitch(int pitch){
        int min_dist = 1000;
        int nearest_pitch = -1;
        for (auto note : s_allowed_pitches_full_set) {
            int dist = abs(note - pitch);
            if (dist < min_dist) {
                min_dist = dist;
                nearest_pitch = note;
            }
        }
        return nearest_pitch;
    }
    

    // A function that takes a note in and returns a note out. The note out is quantized to the nearest allowed pitch.
    int quantize_note(const int& a_note)
    {
        if(s_allowed_pitches_set.empty()) {
            cout << "No allowed notes set. Please set allowed notes first." << endl;
            return a_note;
        }

        // Return the nearest pitch
        return find_nearest_pitch(a_note);
    };

    // List of all playing notes
    std::vector<Note> s_playing_notes = {};

    // Static list of all instances of the class
    static std::set<repitch*> s_instances;

    // Static list of all allowed pitches
    static std::set<int> s_allowed_pitches_set;
    static std::set<int> s_allowed_pitches_full_set;

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
// Init the set of allowed pitches
std::set<int> repitch::s_allowed_pitches_set = {};
std::set<int> repitch::s_allowed_pitches_full_set = {};

long Note::s_counter = 0;





MIN_EXTERNAL(repitch);
