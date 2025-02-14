autowatch = 1
inlets = 1;
outlets = 3;

var debug = false;
var initialised = false;

var api;

var current_track;

var selected_parameter_id;
var selected_parameter_observer;

var arrangement_clips = [];

var clipDict = new Dict(jsarguments[1]);

var getNoteParams = new Dict();


// ---------------------------------------------------------------------------------
// Debug Methods
function log() {
	for(var i=0,len=arguments.length; i<len; i++) {
		var message = arguments[i];
		if(message && message.toString) {
			var s = message.toString();
			if(s.indexOf("[object ") >= 0) {
				s = JSON.stringify(message);
			}
			post(s);
		}
		else if(message === null) {
			post("<null>");
		}
		else {
			post(message);
		}
	}
	post("\n");
}

function post_dict(dictname, keys)
{
	post("Info regarding the dictionary named '", dictname, "': ");
	post();
	post("    Keys: " + keys);
	post();
}

// ---------------------------------------------------------------------------------
// Internal Methods
function init(){
    api = new LiveAPI('live_set');
    // path = this.patcher.filepath;//.replace(/LiveControl.amxd/, '');
	// log('path:', path);

	// Set the current track to the track containing this device
	current_track = new Track('this_device canonical_parent');
	// Post the current track info
	current_track.postInfo();

	if(!initialised){
		arrangement_clips_listener = new LiveAPI(arrangement_clips_changed, current_track.getPath());
		arrangement_clips_listener.property = 'arrangement_clips';
		getNoteParams.clear();
		getNoteParams.set('from_pitch', 0  );
		getNoteParams.set('pitch_span', 127);
		getNoteParams.set('from_time', 0   );
		// getNoteParams.set('return', "start_time", "duration", "pitch", "velocity", "mute", "probability");
	}

	// Add all the arrangement clips to the array of arrangement clips
	clipIDs = current_track.getIDs('arrangement_clips');
	// log('current_track.getIDs(\'arrangement_clips\'):', clipIDs);

	// Fill the arrangement_clips array with Clip objects
	update_clips();

	initialised = true;
    post('initialised\n')
}

function arrangement_clips_changed(args){
	if(!initialised) return;

	// log('arrangement_clips_changed:', args);
	clipIDs = current_track.getIDs('arrangement_clips');
	// log('current_track.getIDs(\'arrangement_clips\'):', clipIDs);

	// Fill the arrangement_clips array with Clip objects
	update_clips();
}

function update_clips(){
	clipDict.clear();
	arrangement_clips = [];
	outlet(0, 'clear_clips');
	for(i=0; i<clipIDs.length; i++){
		output_list = ['add_clip'];

		clip = new Clip(clipIDs[i]);
		// post('clip:', clip.getName(), clip.getProperty('start_time'), clip.getProperty('end_time'), '\n');
		arrangement_clips.push(clip);

		// Get the clip properties
		var clip_id = clip.getID();
		var clip_name = clip.getName();
		var clip_muted = clip.getProperty('muted')[0];
		var clip_start_time = clip.getProperty('start_time')[0];
		var clip_end_time = clip.getProperty('end_time')[0];
		var clip_start_marker = clip.getProperty('start_marker')[0];
		var clip_end_marker = clip.getProperty('end_marker')[0];
		var clip_looping = clip.getProperty('looping')[0];
		var clip_loop_start = clip.getProperty('loop_start')[0];
		var clip_loop_end = clip.getProperty('loop_end')[0];

		// Add the clip properties to the output list
		output_list.push(clip_id);             // 0
		output_list.push(clip_name);           // 1
		output_list.push(clip_muted);          // 2
		output_list.push(clip_start_time);     // 3
		output_list.push(clip_end_time);       // 4
		output_list.push(clip_start_marker);   // 5
		output_list.push(clip_end_marker);     // 6
		output_list.push(clip_looping);        // 7
		output_list.push(clip_loop_start);     // 8
		output_list.push(clip_loop_end);       // 9

		// Parameters for the get_notes_extended call
		getNoteParams.set('from_pitch', 0);
		getNoteParams.set('pitch_span', 128);
		getNoteParams.set('from_time', 0);
		getNoteParams.set('time_span', clip.getLength()[0]);

		// Get the notes for the clip
		getNoteParams.set('time_span', clip.getLength()[0]);
		var notesJson = clip.api.call('get_notes_extended',getNoteParams);
		var notesObject = JSON.parse(notesJson);
		var notesArray = notesObject.notes;
		notesArray.sort(sortByStartTime);
			
		// We need to make a new notes array with the notes that are not muted. Furthermore, we need to take care of the start_marker and end_marker and also the looping.
		// The first notes are the ones following the start_marker. When looping is on and the loop_end is crossed, the notes are repeated.
		
		notesArray.forEach(function(note){
			// printNote(note);	
			// output_list.push(note.note_id);
			if(note.mute == 0){
				output_list.push(note.pitch);
				output_list.push(note.start_time);
				output_list.push(note.duration);
				output_list.push(note.velocity);
				
				// output_list.push(note.probability);
				// output_list.push(note.velocity_deviation);
				// output_list.push(note.release_velocity);	
				var note_start_abs = clip_start_time - clip_start_marker + note.start_time;

				post('note', note.pitch, 'start_abs:', note_start_abs  ,'\n');

			}
		});

		outlet(0, output_list);

	}

	// Move all the notes from the clipDict to the output
	// var keys = clipDict.getkeys();
	// if (keys) {
	// 	for (var j = 0; j < keys.length; j++) {
	// 		var key = keys[j];
	// 		var noteData = clipDict.get(key);
	// 		// log(noteData);
	// }

}

function sortByStartTime(a, b) {
  if (a.start_time < b.start_time) return -1;
  if (a.start_time > b.start_time) return 1;
  return 0;
}

function printNote(note) {
	post("note_id: " + note.note_id + "\n")
	post("release_velocity: " + note.release_velocity + "\n")
	post("velocity_deviation: " + note.velocity_deviation + "\n")
	post("probability: " + note.probability + "\n")
	post("mute: " + note.mute + "\n")
	post("velocity: " + note.velocity + "\n")
	post("duration: " + note.duration + "\n")
	post("start_time: " + note.start_time + "\n")
	post("pitch: " + note.pitch + "\n")
}


// ---------------------------------------------------------------------------------
// LiveObject Class
function LiveObject(path){
	if(!path)
		this.api = new LiveAPI();
	else if(typeof path === 'number'){
		this.api = new LiveAPI('id '+path);
		this._refresh_object();
	}
	else {
		this.api = new LiveAPI(path);
		this._refresh_object();
	}
}
LiveObject.prototype.setPath = function(path){
	this.api.path = path;
	this._refresh_object();
}
LiveObject.prototype.getPath = function(){
	return this.api.unquotedpath;
}
LiveObject.prototype.setID = function(id){
	this.api.id = id;
	this._refresh_object();
}
LiveObject.prototype.getID = function(){
	return parseInt(this.api.id, 10);
}

LiveObject.prototype._refresh_object = function(){
	this.id = parseInt(this.api.id, 10);
	this.path = this.api.unquotedpath;
}

LiveObject.prototype.setProperty = function(property, value){
	this.api.set(property, value);
}
LiveObject.prototype.getProperty = function(property){
	return this.api.get(property);
}

LiveObject.prototype.destroy = function(){
	log(this.token, this, 'Destroyed')
	if(this.observers){
		while( this.observers.length > 0 ) this.observers.pop().id = 0;
		this.observers = null;
	}
	this.api.id = 0;
}

LiveObject.prototype.observe = function(prop, active){
	if(!this.observers) this.observers = [];
	prop = prop.toString();
	indexOfProp = this._getIndexOfObserver(prop);
	post('indexOfProp: ' + indexOfProp + '\n');
	
	if( active && (indexOfProp == -1) ){
		newAPI = new LiveAPI(this._callback, this.api.path);
		newAPI.property = prop;
		this.observers.push(newAPI);
		log(prop + ' observer created\n');
	}
	else if(!active && (indexOfProp != -1) ){
		this.observers[indexOfProp].id = 0;
		this.observers.splice(indexOfProp,1);
		log(prop + ' observer removed\n');
	}	
}

LiveObject.prototype._getIndexOfObserver = function(prop){
	for(i=0; i<this.observers.length; i++){
		if(this.observers[i].property == prop) return i;
	}
	return -1;
}

LiveObject.prototype._callback = function(args){
	log('live_object: observer.callback called with args: ' + args);
}

LiveObject.prototype.getName = function(){
	return this.api.get('name')[0];
}

// Get the childern as an array of ints
LiveObject.prototype.getIDs = function(child){
	temp = [];
	childcount = this.api.getcount(child);
	if(childcount > 0){
		this.api.get(child).forEach(function(item){
			if(!isNaN(item)) temp.push(parseInt(item,10));
		});
	}
	return temp;
}


// ---------------------------------------------------------------------------------
// Song Class
function Song(){
	this.base = LiveObject;
	this.base('live_set');
}
Song.prototype = new LiveObject;

Song.prototype._callback = function(args){
	log('song: observer.callback called with args: ' + args);
}

Song.prototype.stop_all_clips = function(quantized){
	if(!quantized) quantized=1;
	this.api.call('stop_all_clips', quantized);
}

function track_fireClip(){
	var a = arrayfromargs(arguments);
	arg = a[Math.floor(Math.random() * a.length)];
	current_track.fireClip(arg);
}
function track_clipSlotIDs(){
	log('current_track.getIDs(\'clip_slots\'):', current_track.getIDs('clip_slots'));
}


// ---------------------------------------------------------------------------------
// Track Class
function Track(path){
	this.base = LiveObject;
	this.base(path);
}
Track.prototype = new LiveObject;

Track.prototype._callback = function(args){
	log('track: observer.callback called with args: ' + args);
}

Track.prototype.stop_all_clips = function(){
	this.api.call('stop_all_clips');
}
	
Track.prototype.postInfo = function(){
	post('Track Path: ', this.getPath(), '\n');
	post('Track ID: ', this.getID(), '\n');
	post('Track Name: ', this.getName(), '\n');
}

Track.prototype.getMixerDevice = function(){
	if(!this.mixer_device) this.mixer_device = new LiveAPI(this.api.get('mixer_device'));
	return this.mixer_device;
}

Track.prototype.setVolume = function(value){
	api.id = this.api.get('mixer_device')[1];
	api.id = api.get('volume')[1];
	minimum = api.get('min');
	maximum = api.get('max');
	if(value < minimum) value = minimum;
	else if(value > maximum) value = maximum;
	api.set('value', value);
}

Track.prototype.getVolume = function(){
	api.id = this.api.get('mixer_device')[1];
	api.id = api.get('volume')[1];
	return api.get('value');
}
Track.prototype.setPan = function(value){
	api.id = this.api.get('mixer_device')[1];
	api.id = api.get('panning')[1];
	minimum = api.get('min');
	maximum = api.get('max');
	if(value < minimum) value = minimum;
	else if(value > maximum) value = maximum;
	api.set('value', value);
}

Track.prototype.getPan = function(){
	api.id = this.api.get('mixer_device')[1];
	api.id = api.get('panning')[1];
	return api.get('value');
}

Track.prototype.setSend = function(index, value){
	api.id = this.api.get('mixer_device')[1];
	api.id = api.get('sends')[index*2+1];
	api.set('value', value);
}

Track.prototype.getSend = function(index){
	api.id = this.api.get('mixer_device')[1];
	api.id = api.get('sends')[index*2+1];
	return api.get('value');
}
Track.prototype.setOutput = function(output){
	this.api.set('current_output_routing', output);
}
Track.prototype.setInput = function(input){
	this.api.set('current_input_routing', input);
}

Track.prototype.fireClip = function(arg){	
	if(typeof arg === 'number'){
		api.path = this.getPath() + ' clip_slots ' + arg;
		api.call('fire');
		// log('new path', path);
		return;
	}
	else {
		num_clip_slots = this.api.getcount('clip_slots');
		for(i=0; i<num_clip_slots; i++){
			api.path = this.getPath() + ' clip_slots ' + i;
			if(api.get('has_clip') != 0){
				api.id = api.get('clip')[1];
				if(api.get('name') == arg){
					api.call('fire');
					return;
				}
			}
		}
	}
}
 

Array.prototype.getRandElement = function(){
	return this[Math.floor(Math.random() * this.length)];
}

//--------------------------------------------------------------------
// Device class
function Device(path){
	this.base = LiveObject;
	this.base(path);
}

Device.prototype = new LiveObject;

//--------------------------------------------------------------------
// DeviceParameter class
function DeviceParameter(path){
	this.base = LiveObject;
	this.base(path);
}
DeviceParameter.prototype = new LiveObject;

DeviceParameter.prototype.randomize = function(minimum, maximum){
	if(!minimum) minimum = this.api.get('min');
	if(!maximum) maximum = this.api.get('max');
	newValue = minimum + (Math.random()*(maximum-minimum));
	this.api.set('value', newValue);
}

function rand_current_param(){
	dp = new DeviceParameter(selected_parameter_id);
	dp.randomize();
}

//--------------------------------------------------------------------
// Scene class
function Scene(path){
	this.base = LiveObject;
	this.base(path);
}
Scene.prototype = new LiveObject;

Scene.prototype.fire = function(force_legato, can_select_on_launch){
	if(!force_legato) force_legato = 0;
	if(!can_select_on_launch) can_select_on_launch = 1;
	this.api.call('fire', force_legato, can_select_on_launch);
}
Scene.prototype.fire_as_selected = function(force_legato){
	if(!force_legato) force_legato = 0;
	this.api.call('fire_as_selected', force_legato);
	scene('selected');
}
Scene.prototype.set_fire_button_state = function(state){
	this.api.call('set_fire_button_state', state);
}

//--------------------------------------------------------------------
// Clip class
function Clip(path){
	this.base = LiveObject;
	this.base(path);
}
Clip.prototype = new LiveObject;

Clip.prototype.getLength = function() {
	return this.api.get('length');
}

Clip.prototype._parseNoteData = function(data) {
	var notes = [];
	// data starts with "notes"/count and ends with "done" (which we ignore)
	for(var i=2,len=data.length-1; i<len; i+=6) {
		// and each note starts with "note" (which we ignore) and is 6 items in the list
		var note = new Note(data[i+1], data[i+2], data[i+3], data[i+4], data[i+5]);
		notes.push(note);
	}
	notes.sort(function(a, b) {
		return a.start - b.start;
	});
	return notes;
}

Clip.prototype.getSelectedNotes = function() {
	var data = this.api.call('get_selected_notes');
	return this._parseNoteData(data);
}

Clip.prototype.getNotes = function(startTime, timeRange, startPitch, pitchRange){
	if(!startTime) startTime = 0;
	if(!timeRange) timeRange = this.getLength();
	if(!startPitch) startPitch = 0;
	if(!pitchRange) pitchRange = 128;

	var data = this.api.call("get_notes", startTime, startPitch, timeRange, pitchRange);
	return this._parseNoteData(data);
}

 
Clip.prototype._sendNotes = function(notes) {
	var api = this.api;
 
	api.call("notes", notes.length);

	notes.forEach(function(note) {
		api.call("note", note.getPitch(),
		note.getStart(), note.getDuration(),
		note.getVelocity(), note.getMuted());
	});
	api.call('done');
}

Clip.prototype.replaceSelectedNotes = function(notes) {
	this.api.call("replace_selected_notes");
	this._sendNotes(notes);
}

Clip.prototype.setNotes = function(notes) {
	this.api.call("set_notes");
	this._sendNotes(notes);
}

Clip.prototype.selectAllNotes = function() {
	this.api.call("select_all_notes");
}

Clip.prototype.replaceAllNotes = function(notes) {
	this.selectAllNotes();
	this.replaceSelectedNotes(notes);
}

//--------------------------------------------------------------------
// Note class

function Note(pitch, start, duration, velocity, muted) {
	this.pitch = pitch;
	this.start = start;
	this.duration = duration;
	this.velocity = velocity;
	this.muted = muted;
}

Note.prototype.toString = function() {
	return '{pitch:' + this.pitch +
		', start:' + this.start +
		', duration:' + this.duration +
		', velocity:' + this.velocity +
		', muted:' + this.muted + '}';
}

Note.MIN_DURATION = 1/128;

Note.prototype.getPitch = function() {
	if(this.pitch < 0) return 0;
	if(this.pitch > 127) return 127;
	return this.pitch;
}

Note.prototype.getStart = function() {
	// we convert to strings with decimals to work around a bug in Max
	// otherwise we get an invalid syntax error when trying to set notes
	if(this.start <= 0) return "0.0";
	return this.start.toFixed(4);
}

Note.prototype.getDuration = function() {
	if(this.duration <= Note.MIN_DURATION) return Note.MIN_DURATION;
	return this.duration.toFixed(4); // workaround similar bug as with getStart()
}

Note.prototype.getVelocity = function() {
	if(this.velocity < 0) return 0;
	if(this.velocity > 127) return 127;
	return this.velocity;
}

Note.prototype.getMuted = function() {
	if(this.muted) return 1;
	return 0;
}
