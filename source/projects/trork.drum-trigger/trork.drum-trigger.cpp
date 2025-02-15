/// @file
///	@ingroup 	robotic-audio 
///	@copyright	Copyright 2025 Hjalte Bested Hjorth. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <set>
#include <map>

using namespace c74::min;

// A class to represent a note. A note that can be repitched, therefore we store both pitch_in and pitch_out.


class LiveSet {
public:
    LiveSet() = default;

    // Setters
    void set_tempo     (double a_tempo   ) { tempo      = a_tempo;      }
    void set_beats     (double a_beats   ) { 
        last_beats = beats < last_beats ? beats - 0.02 : beats;
        beats = a_beats;
    }
    void set_is_playing(bool a_is_playing) { is_playing = a_is_playing; }

    // Getters
    double get_tempo()      const { return tempo; }
    double get_beats()      const { return beats; }
    double get_last_beats() const { return last_beats; }
    bool   get_is_playing() const { return is_playing; } 

private:    
    double tempo = 120.0;      // The tempo of the Live Set, in BPM.
    double beats = -1.0;       // The current playing position in the Live Set, in beats.
    double last_beats = -1.0;  // The last playing position in the Live Set, in beats.
    bool   is_playing = false; // Is Live's transport is running?
};

class Track {
public:
    class Clip {
    public:
        class Note {
        public:
            // Note Constructor
            Note() : pitch(-1), start_time(-1.0), duration(-1.0), velocity(-1), mute(false), id(-1) {}
            Note(int a_pitch, double a_start_time, double a_duration, int a_velocity, bool a_mute) : pitch(a_pitch), start_time(a_start_time), duration(a_duration), velocity(a_velocity), mute(a_mute), id(++s_counter) {}
            
            // Stream operator to print the note
            friend std::ostream& operator<<(std::ostream& os, const Note& note) {
            os << "Note:(" << note.pitch << "," << note.start_time << "," << note.duration << "," << note.velocity << "," << note.mute << "," << note.id <<")";
            return os;
            }

            // Return the actual start time of the note in beats, taking into account the offset and the tempo
            double get_actual_start_time(double tempo) const {
            }

            // Check if the note is playing at the given time
            bool playing(double time, double time_offset = 0.0) const {
                double actual_start_time = start_time + time_offset;
                return actual_start_time <= time && actual_start_time + duration > time;
            }
            
            // Note Member variables
            int pitch = -1;
            double start_time = -1;
            double duration = -1;
            int velocity = -1;
            bool mute = false;
            long id = -1;
            static long s_counter;
        };;

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

        double clip_duration()         const { return m_end_time - m_start_time;                                          }
        double clip_loop_duration()    const { return m_loop_end - m_loop_start;                                          }
        double clip_time_before_loop() const { return m_loop_end - m_start_marker;                                        }
        double loopCount()             const { return (clip_duration() - clip_time_before_loop()) / clip_loop_duration(); }
        bool inLoopRegion(const Note& note)  const { return m_looping && note.start_time >= m_loop_start && note.start_time < m_loop_end; }

        // Add a note to the clip
        void add_note(int a_pitch, double a_start_time, double a_duration, int a_velocity, bool a_mute) {
            m_notes.push_back(Note(a_pitch, a_start_time, a_duration, a_velocity, a_mute));
        }

        // Compute the absolute time of the notes, taking into account the clip start_time, end_time, start_marker, end_marker and looping
        void add_to_track_notes(vector<Note>& track_notes) 
        {
            // If the clip is muted, we skip it
            if(m_muted) return;

            // For each note in the clip
            for(const Note& clipNote : m_notes) {
                // If the clipNote is Mute, we skip it
                if(clipNote.mute) continue;
                // If the clipnote is outside the clip, we skip it
                if(clipNote.start_time < 0) continue;
                if(clipNote.start_time >= clip_duration()) continue;
                // New note for the track. This note will have an absolute time.
                Note trackNote = clipNote;
                // Adjust the start time of the note according to the start marker
                trackNote.start_time -= m_start_marker;
                // If the new start time is negative, we skip the note
                if(trackNote.start_time < 0) continue;
                // If the clip is looping and the note is after the loop region, we skip the note
                if(m_looping && trackNote.start_time >= clip_time_before_loop()) continue;
                // Now we need to check if the note is in the loop region
                const bool inLoop = inLoopRegion(clipNote);

                trackNote.start_time += m_start_time;

                // We can now push the note to the track_notes vector
                track_notes.push_back(trackNote);

                // If the note is in the loop region, we need to add the note to the track_notes vector for each loop iteration
                if(!inLoop) continue;

                // For each loop iteration
                int numLoops = ceil(loopCount());
                for(int i=1; i<=numLoops; i++) {
                    // Create a new note
                    Note loopNote = trackNote;
                    // Adjust the start time of the note according to the loop
                    loopNote.start_time += i * clip_loop_duration();
                    // If the new start time is after the end marker, we skip the note
                    if(loopNote.start_time >= m_end_time) continue;
                    // Push the note to the track_notes vector
                    track_notes.push_back(loopNote);
                }
            }
        }

        // Stream operator to print the clip
        friend std::ostream& operator<<(std::ostream& os, const Clip& clip){
            os << "Clip:(" << clip.m_name << "," << clip.m_start_time << "," << clip.m_end_time <<  "," << clip.m_start_marker << "," << clip.m_end_marker << "," << clip.m_looping << "," << clip.m_loop_start << "," << clip.m_loop_end << "," << clip.m_notes.size() << ")";
            os << "Notes:(";
            for(auto& note : clip.m_notes) {
                os << note << ",";
            }
            os << ")"; 
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

    Track() = default;

    void clear() {
        m_clips.clear();
    }

    void collect_track_notes() {
        m_notes.clear();
        // Add all the notes from all the clips to the notes vector
        for(auto& clip : m_clips) {
            clip.add_to_track_notes(m_notes);
        }
        // Sort the notes vector by start time
        std::sort(m_notes.begin(), m_notes.end(), [](const Clip::Note& a, const Clip::Note& b) { return a.start_time < b.start_time; });
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

        // Collect the notes from the clips - in absolte time with looping
        collect_track_notes();
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
    friend std::ostream& operator<<(std::ostream& os, const Track& s_track) {
        os << "Track:(";
        for(auto& clip : s_track.m_clips) {
            os << clip << ",";
        }
        os << ")";
        return os;
    }

    // Calculate the absolute time of the notes and add them to the notes vector
    void calculate_notes(double time) {
        m_notes.clear();
        for(auto& clip : m_clips) {
            if(clip.m_start_time <= time && clip.m_end_time >= time) {
                for(auto& note : clip.m_notes) {
                    if(note.start_time <= time && note.start_time + note.duration >= time) {
                        m_notes.push_back(note);
                    }
                }
            }
        }
    }

    std::vector<Clip> m_clips;
    const Clip NoClip = Clip();
    std::vector<Clip::Note> m_notes;
    std::set<Clip::Note*> m_playing_note_ptrs;

};

long Track::Clip::Note::s_counter = 0;

class DrumTrigger {
public:
    DrumTrigger() = default;
    DrumTrigger(int a_pitch_in, int a_pitch_out, int a_velocity_min=0, int a_velocity_max=127, std::string a_name="") : pitch_in(a_pitch_in), pitch_out(a_pitch_out), velocity_min(a_velocity_min), velocity_max(a_velocity_max), name(a_name) {}


    // Get scaled velocity
    int get_velocity(int velocity) const {
        return velocity_min + (velocity_max - velocity_min) * velocity / 127;
    }

    // Equality operator
    bool operator==(const DrumTrigger& other) const {
        return pitch_in == other.pitch_in;
    }

    // Sort operator
    bool operator<(const DrumTrigger& other) const {
        return pitch_in < other.pitch_in;
    }

    // Stream operator to print the drum trigger
    friend std::ostream& operator<<(std::ostream& os, const DrumTrigger& drum_trigger) {
        os << "DrumTrigger:(" << drum_trigger.name << "," << drum_trigger.pitch_in << "," << drum_trigger.pitch_out << "," << drum_trigger.velocity_min << "," << drum_trigger.velocity_max << ")";
        return os;
    }

    // Member variables
    int pitch_in = -1;
    int pitch_out = -1;
    int velocity_min = 0;
    int velocity_max = 127;
    std::string name = "";

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
        setup_drum_triggers();
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
        bool is_playing = s_live_set.get_is_playing();
        if(!is_playing) flush();
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

    // Attribute to set mute
    attribute<bool> mute {this, "mute", false,
        description {"Mute the drum triggers."},
        setter {MIN_FUNCTION {
            return args;
        }}
    };

    void noteOn(Track::Clip::Note note) {
        // Play the note
        out1.send("note", note.pitch, note.velocity);
        
        // If the m_drum_triggers contains a drum trigger with the same pitch_in as the note.pitch, we play the drum trigger. Use find_if
        auto it = std::find_if(m_drum_triggers.begin(), m_drum_triggers.end(), [note](const DrumTrigger& drum_trigger) {
            return drum_trigger.pitch_in == note.pitch;
        });

        // A matching drum trigger was found
        if(it != m_drum_triggers.end()) {
            out1.send("trig", it->pitch_out, it->get_velocity(note.velocity));
        }

    }

    void noteOff(Track::Clip::Note note) {
        // Send note off
        out1.send("note", note.pitch, 0);
    }


    // The playing position in the Live Set, in beats.
    message<threadsafe::yes> number { this, "number", "The playing position in the Live Set, in beats.", 
        MIN_FUNCTION {
            s_live_set.set_beats(static_cast<double>(args[0]) - offset);

            if(mute) return {};

            // Print the notes between last_beats and beats
            double last_beats = s_live_set.get_last_beats();
            double beats = s_live_set.get_beats();


            for(auto& note : s_track.m_notes)
            {   
                // Get a pointer to the track note
                auto note_ptr = &note;

                // Initialize offset_beats to 0.0
                double offset_beats = 0.0;
                double offset_ms = m_time_offsets[note.pitch];
                
                // If the offset_ms is not 0.0, we calculate the offset_beats
                if(offset_ms != 0.0) offset_beats = offset_ms / 60000.0 * s_live_set.get_tempo();
                
                // Check if the note should play
                bool noteShouldPlay = note.playing(beats, offset_beats);
                bool noteIsPlaying = s_track.m_playing_note_ptrs.find(note_ptr) != s_track.m_playing_note_ptrs.end();
                if(noteShouldPlay && !noteIsPlaying) {
                    // Play the note
                    s_track.m_playing_note_ptrs.insert(note_ptr);
                    noteOn(note);
                }
                else if(!noteShouldPlay && noteIsPlaying) {
                    // Stop the note
                    s_track.m_playing_note_ptrs.erase(note_ptr);
                    noteOff(note);
                }            
            }
            return {};
        }  
    };

    // Flush all the playing notes
    message<> flush { this, "flush", "Flush all the playing notes.",
        MIN_FUNCTION {
            for(auto note_ptr : s_track.m_playing_note_ptrs) {
                out1.send("note", note_ptr->pitch, 0);
            }
            s_track.m_playing_note_ptrs.clear();
            return {};
        }  
    };    

    // Set Offset for a given pitch
    message<> set_time_offset { this, "set_time_offset", "Set the time offset in ms for a given pitch. Negative values will play the note earlier.",
        MIN_FUNCTION {
            m_time_offsets[args[0]] = args[1];
            return {};
        }  
    };

    // Cleat time offsets
    message<> clear_time_offsets { this, "clear_time_offsets", "Clear the time offsets.",
        MIN_FUNCTION {
            for(int i=0; i<128; i++) {
                m_time_offsets[i] = 0.0;
            }
            return {};
        }  
    };

    // Print the time offsets
    message<> print_time_offsets { this, "print_time_offsets", "Print the time offsets.",
        MIN_FUNCTION {
            cout << "Time Offsets:" << endl;
            for(int i=0; i<128; i++) {
                cout << i << ": " << m_time_offsets[i] << endl;
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

    // message to clear the clips
    message<> clear_clips { this, "clear_clips", "Clear the clips.",
        MIN_FUNCTION {
            s_track.clear();
            return {};
        }  
    };

    // message to add a clip
    message<> add_clip { this, "add_clip", "Add a clip.",
        MIN_FUNCTION {
            s_track.from_atoms(args);
            return {};
        }  
    };

    // message to print the clips
    message<> print_clips { this, "print_clips", "Print the clips.",
        MIN_FUNCTION {
            cout << s_track << endl;
            return {};
        }  
    };

    // message to print the clips
    message<> print_track_notes { this, "print_track_notes", "Print all notes on the track.",
        MIN_FUNCTION {
            
            for(auto& note : s_track.m_notes) {
                cout << note << endl;
            }

            return {};
        }  
    };

    // Setup a single Drum Trigger
    message<> setup_drum_trigger { this, "setup_drum_trigger", "Setup a single drum trigger. args: pitch_in, pitch_out, velocity_min, velocity_max, delay, name",
        MIN_FUNCTION {
            m_drum_triggers.insert(DrumTrigger{args[0], args[1], args[2], args[3], args[5]});
            m_time_offsets[args[0]] = args[4];
            return {};
        }  
    };

    // Setup Drum Trigger
    message<> setup_drum_triggers { this, "setup_drum_triggers", "Setup the drum trigger.",
        MIN_FUNCTION {
            m_drum_triggers.clear();
            m_drum_triggers.insert(DrumTrigger{36, 36, 35, 80, "TopDrum" }); m_time_offsets[36] = -40;
            m_drum_triggers.insert(DrumTrigger{38, 37, 50, 90, "SideDrum"}); m_time_offsets[38] = -65;
            m_drum_triggers.insert(DrumTrigger{51, 38, 20, 40, "Frog"    }); m_time_offsets[51] = -35;
            m_drum_triggers.insert(DrumTrigger{41, 40, 10, 30, "Cabasa"  }); m_time_offsets[41] = -15;
            m_drum_triggers.insert(DrumTrigger{40, 39, 10, 30, "Cabasa2" }); m_time_offsets[40] = -15;
            return {};
        }  
    };
    // Clear Drum Triggers
    message<> clear_drum_triggers { this, "clear_drum_triggers", "Clear the drum triggers.",
        MIN_FUNCTION {
            m_drum_triggers.clear();
            return {};
        }  
    };


private:

    // Static list of all instances of the class
    static std::set<trork_drum_trigger*> s_instances;
    
    // Static Track
    static Track s_track;

    // A structure to hold information about the live set
    static LiveSet s_live_set;

    // A note offset for each pitch
    double m_time_offsets[128] = {0.0};

    // Pitch Remapping for Drums
    std::set<DrumTrigger> m_drum_triggers = {};

};


// Init Static variables
std::set<trork_drum_trigger*> trork_drum_trigger::s_instances = {};

// Init Track
Track trork_drum_trigger::s_track = Track();


// Init Static Beat
LiveSet trork_drum_trigger::s_live_set = LiveSet();


MIN_EXTERNAL(trork_drum_trigger);
