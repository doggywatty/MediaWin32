/// @description Start monitoring (delayed so RegisterCallbacks has fired).
var _ret = StartMediaMonitor();
monitoring = true;
show_debug_message("[MediaWin32Demo] StartMediaMonitor() -> " + string(_ret));
