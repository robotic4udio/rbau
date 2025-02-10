autowatch = 1
inlets = 1;
outlets = 3;

var debug = true;
var initialised = false;

var api;

var track_count;
var tracks = [];
var return_track_count;
var return_tracks = [];
var master_track;

var current_track;

var scene_count;
var scenes = [];
var current_scene;

var selected_parameter_id;
var selected_parameter_observer;

var selected_scene_id;

var selected_track_id;
var autoselect_track = false;

var detail_clip_id;

var arrangement_clips = [];

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

	log('arrangement_clips_changed:', args);
	clipIDs = current_track.getIDs('arrangement_clips');
	log('current_track.getIDs(\'arrangement_clips\'):', clipIDs);

	// Fill the arrangement_clips array with Clip objects
	update_clips();
}

function update_clips(){
	output_list = [];
	arrangement_clips = [];
	for(i=0; i<clipIDs.length; i++){
		clip = new Clip(clipIDs[i]);
		// post('clip:', clip.getName(), clip.getProperty('start_time'), clip.getProperty('end_time'), '\n');
		arrangement_clips.push(clip);
		output_list.push(clip.getName());
		output_list.push(clip.getProperty('start_time')[0]);
		output_list.push(clip.getProperty('end_time')[0]);
	}
	outlet(0, output_list);

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
  
//--------------------------------------------------------------------
// Humanize behavior
 
function humanize(type, maxTimeDelta, maxVelocityDelta) {
  var humanizeVelocity = false,
      humanizeTime = false;
  
  switch(type) {
    case "velocity": humanizeVelocity = true; break;
    case "time": humanizeTime = true; break;
    default: humanizeVelocity = humanizeTime = true;
  }
  
  if(!maxTimeDelta) maxTimeDelta = 0.05;
  if(!maxVelocityDelta) maxVelocityDelta = 5;
   
  clip = new Clip();
  notes = clip.getSelectedNotes();
  notes.forEach(function(note) {
    if(humanizeTime) note.start += maxTimeDelta * (2*Math.random() - 1);
    if(humanizeVelocity) note.velocity += maxVelocityDelta * (2*Math.random() - 1);
  });
  clip.replaceSelectedNotes(notes);
}