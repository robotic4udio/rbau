/// @file
///	@ingroup 	robotic-audio 
///	@copyright	Copyright 2025 Hjalte Bested Hjorth. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <set>

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
    double tempo = 120.0;      // The tempo of the Live Set, in BPM.
    double beats = -1.0;       // The current playing position in the Live Set, in beats.
    bool   is_playing = false; // Is Live's transport is running?
};

class Clips {
public:
    class Clip {
    public:
        class Note {
        public:
            // Note Constructor
            Note() : pitch(-1), start_time(-1), duration(-1), velocity(-1), mute(false) {}
            Note(int a_pitch, int a_start_time, int a_duration, int a_velocity, bool a_mute) : pitch(a_pitch), start_time(a_start_time), duration(a_duration), velocity(a_velocity), mute(a_mute) {}
            
            // Stream operator to print the note
            friend std::ostream& operator<<(std::ostream& os, const Note& note) {
                os << "Note:(" << note.pitch << "," << note.start_time << "," << note.duration << "," << note.velocity << "," << note.mute << ")";
                return os;
            }
            
            // Note Member variables
            int pitch = -1;
            int start_time = -1;
            int duration = -1;
            int velocity = -1;
            bool mute = false;
        };

        Clip() : m_id(-1), m_name(""), m_muted(false), m_start_time(-1), m_end_time(-1), m_start_marker(-1), m_end_marker(-1), m_looping(false), m_loop_start(-1), m_loop_end(-1) {}
        Clip(int a_id, std::string a_name, bool a_muted, double a_start_time, double a_end_time, double a_start_marker, double a_end_marker, bool a_looping, double a_loop_start, double a_loop_end) : m_id(a_id), m_name(a_name), m_muted(a_muted), m_start_time(a_start_time), m_end_time(a_end_time), m_looping(a_looping), m_loop_start(a_loop_start), m_loop_end(a_loop_end) {}

        // Init
        void init() {
            m_id = -1;
            m_name = "";
            m_muted = false;
            m_start_time = -1;
            m_end_time = -1;
            m_start_marker = -1;
            m_end_marker = -1;
            m_looping = false;
            m_loop_start = -1;
            m_loop_end = -1;
            m_notes.clear();
        }

        // Add a note to the clip
        void add_note(int a_pitch, int a_start_time, int a_duration, int a_velocity, bool a_mute) {
            m_notes.push_back(Note(a_pitch, a_start_time, a_duration, a_velocity, a_mute));
        }

        // Stream operator to print the clip
        friend std::ostream& operator<<(std::ostream& os, const Clip& clip){
            os << "Clip:(" << clip.m_name << "," << clip.m_start_time << "," << clip.m_end_time <<  "," << clip.m_start_marker << "," << clip.m_end_marker << "," << clip.m_looping << "," << clip.m_loop_start << "," << clip.m_loop_end << "," << clip.m_notes.size() << ")";
            // os << "Notes:(";
            // for(auto& note : clip.m_notes) {
            //     os << note << ",";
            // }
            // os << ")"; 
            return os;
        }

        // Member variables
        int               m_id;
        std::string       m_name;
        bool              m_muted;
        double            m_start_time;
        double            m_end_time;
        double            m_start_marker;
        double            m_end_marker;
        bool              m_looping;
        double            m_loop_start;
        double            m_loop_end;
        std::vector<Note> m_notes;

    };

    Clips() = default;

    void clear() {
        m_clips.clear();
    }

    void from_atoms(const atoms& args){
        //  Make a new clip
        Clip clip(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
        
        // Add notes to the clip
        for(int i=10; i<args.size(); i+=4) {
            clip.add_note(args[i], args[i+1], args[i+2], args[i+3], false);
        }

        // Add the clip to the track
        m_clips.push_back(clip);
    }

    // Get Clip at time
    const Clip& get_clip_at_time(double time) const {
        for(auto it = m_clips.rbegin(); it != m_clips.rend(); ++it) {
            if(it->m_start_time <= time) return *it;
        }
        // Return an empty clip if no clip is found
        return NoClip;
    }

    // Stream operator to print the clip track
    friend std::ostream& operator<<(std::ostream& os, const Clips& s_clip_track) {
        os << "Clips:(";
        for(auto& clip : s_clip_track.m_clips) {
            os << clip << ",";
        }
        os << ")";
        return os;
    }

private:
    std::vector<Clip> m_clips;
    const Clip NoClip = Clip();
};

class trork_drum_trigger : public object<trork_drum_trigger> {
public:
    MIN_DESCRIPTION	{"Trigger the mechanic instruments before the noteevent actually occours in live to compensate for mechanical latency."};
    MIN_TAGS		{"tromleorkestret"};
    MIN_AUTHOR		{"robotic-4udio"};
    MIN_RELATED		{"js"};

    // Constructor
    trork_drum_trigger(const atoms& args = {}) {
        s_instances.insert(this);
    }

    // Destructor
    ~trork_drum_trigger() { 
        s_instances.erase(this); 
    }

    // Notification types
    enum class notefication_type {
        playing_changed,
        enum_count
    };

    // Notify all instances of a change
    static void notify_all(trork_drum_trigger* notifying_instance, notefication_type type) {
        for (auto instance : s_instances){
            instance->notify(notifying_instance, type);
        }
    }

    // Notify this instance of a change. Pass the type of change as an argument and also the instance that is notifying.
    void notify(trork_drum_trigger* notifying_instance, notefication_type type) {
        switch (type) {
            case notefication_type::playing_changed:
                playing_changed(notifying_instance);
            break;
            case notefication_type::enum_count:
            break;
        }
    }

    // Notification function for playing_changed
    void playing_changed(trork_drum_trigger* notifying_instance)
    {
    }

    // Make inlets and outlets
    inlet<> input{ this, "(anything) post greeting to the max console" };
    outlet<thread_check::scheduler, thread_action::fifo> out1 { this, "(anything) output the message which is posted to the max console" };

    // Attribute to offset the beats time from the Live Set
    attribute<double> offset {this, "offset", 0.0,
        description {"Offset the beats time from the Live Set."},
        range {-1.0, 1.0},  // Define a range for the parameter
        setter {MIN_FUNCTION {
            return args;
        }}
    };

    // The playing position in the Live Set, in beats.
    message<threadsafe::yes> number { this, "number", "The playing position in the Live Set, in beats.", 
        MIN_FUNCTION {
            s_live_set.set_beats(static_cast<double>(args[0]) - offset);

 
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

    // message to clear the clips
    message<> clear_clips { this, "clear_clips", "Clear the clips.",
        MIN_FUNCTION {
            s_clip_track.clear();
            return {};
        }  
    };

    // message to add a clip
    message<> add_clip { this, "add_clip", "Add a clip.",
        MIN_FUNCTION {
            s_clip_track.from_atoms(args);
            return {};
        }  
    };

    // message to print the clips
    message<> print_clips { this, "print_clips", "Print the clips.",
        MIN_FUNCTION {
            cout << s_clip_track << endl;
            return {};
        }  
    };


private:

    // Static list of all instances of the class
    static std::set<trork_drum_trigger*> s_instances;
    
    // Static Clips
    static Clips s_clip_track;
    static Clips::Clip s_current_clip;

    // A structure to hold information about the live set
    static LiveSet s_live_set;

};


// Init Static variables
std::set<trork_drum_trigger*> trork_drum_trigger::s_instances = {};

// Init Clips
Clips trork_drum_trigger::s_clip_track = Clips();
Clips::Clip trork_drum_trigger::s_current_clip = Clips::Clip();


// Init Static Beat
LiveSet trork_drum_trigger::s_live_set = LiveSet();


MIN_EXTERNAL(trork_drum_trigger);
