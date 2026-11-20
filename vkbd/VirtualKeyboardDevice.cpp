/*
 * Renku virtual keyboard: an input_server device add-on that types what the
 * shell's on-screen keyboard sends to its port, as if a real keyboard had.
 *
 * It also tells the shell when a text field gets or loses focus. Windows tell
 * input_server so for its input methods (IS_FOCUS_IM_AWARE_VIEW and
 * IS_UNFOCUS_IM_AWARE_VIEW), but it passes this on to no one; the add-on runs
 * inside input_server, so it watches these messages with a filter.
 */
#include "VirtualKeyboard.h"

#include <Application.h>
#include <InputServerDevice.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <MessageFilter.h>
#include <Messenger.h>
#include <OS.h>
#include <Roster.h>

#include <input/InputServerTypes.h>

#include <string.h>


class VirtualKeyboardDevice : public BInputServerDevice {
public:
								VirtualKeyboardDevice();
	virtual						~VirtualKeyboardDevice();

	virtual	status_t			InitCheck();
	virtual	status_t			Start(const char* device, void* cookie);
	virtual	status_t			Stop(const char* device, void* cookie);
	virtual	status_t			Control(const char* device, void* cookie,
									uint32 code, BMessage* message);

private:
	static	status_t			_Reader(void* data);
			void				_Modifiers(uint32 from, uint32 to);
			void				_Type(const virtual_key& key);

			port_id				fPort;
			thread_id			fThread;
			BMessageFilter*		fFocusFilter;
};


static filter_result
focus_changed(BMessage* message, BHandler** target, BMessageFilter* filter)
{
	if (message->what != IS_FOCUS_IM_AWARE_VIEW
		&& message->what != IS_UNFOCUS_IM_AWARE_VIEW) {
		return B_DISPATCH_MESSAGE;
	}

	bool text = message->what == IS_FOCUS_IM_AWARE_VIEW;

	// shortcut: Terminal's view takes text without saying so (it isn't
	// B_INPUT_METHOD_AWARE), so Terminal is known by its signature.
	BMessenger view;
	app_info info;
	if (!text && message->FindMessenger("view", &view) == B_OK
		&& be_roster->GetRunningAppInfo(view.Team(), &info) == B_OK
		&& strcmp(info.signature, "application/x-vnd.Haiku-Terminal") == 0) {
		text = true;
	}

	// The window waits for input_server's reply, so don't wait for the shell.
	BMessage notice(kMsgTextFocus);
	notice.AddBool("text", text);
	BMessenger(SHELL_SIGNATURE).SendMessage(&notice, (BHandler*)NULL, 0);
	return B_DISPATCH_MESSAGE;
}


VirtualKeyboardDevice::VirtualKeyboardDevice()
	:
	fPort(-1),
	fThread(-1),
	fFocusFilter(NULL)
{
	// input_server loads its add-ons while it holds its own lock.
	if (be_app->Lock()) {
		fFocusFilter = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE,
			focus_changed);
		be_app->AddCommonFilter(fFocusFilter);
		be_app->Unlock();
	}
}


VirtualKeyboardDevice::~VirtualKeyboardDevice()
{
	if (fFocusFilter != NULL && be_app->Lock()) {
		be_app->RemoveCommonFilter(fFocusFilter);
		be_app->Unlock();
		delete fFocusFilter;
	}
}


status_t
VirtualKeyboardDevice::InitCheck()
{
	static input_device_ref device = {
		(char*)"Renku virtual keyboard", B_KEYBOARD_DEVICE, NULL
	};
	static input_device_ref* devices[] = { &device, NULL };
	RegisterDevices(devices);
	return B_OK;
}


status_t
VirtualKeyboardDevice::Start(const char* device, void* cookie)
{
	fPort = create_port(16, VIRTUAL_KEYBOARD_PORT);
	if (fPort < 0)
		return fPort;

	fThread = spawn_thread(_Reader, "virtual keyboard", B_URGENT_DISPLAY_PRIORITY,
		this);
	if (fThread < 0) {
		delete_port(fPort);
		return fThread;
	}
	return resume_thread(fThread);
}


status_t
VirtualKeyboardDevice::Stop(const char* device, void* cookie)
{
	// Deleting the port ends the reader.
	delete_port(fPort);
	status_t result;
	wait_for_thread(fThread, &result);
	fPort = fThread = -1;
	return B_OK;
}


status_t
VirtualKeyboardDevice::Control(const char* device, void* cookie, uint32 code,
	BMessage* message)
{
	return B_OK;
}


status_t
VirtualKeyboardDevice::_Reader(void* data)
{
	VirtualKeyboardDevice* self = (VirtualKeyboardDevice*)data;
	virtual_key key;
	int32 code;
	while (true) {
		ssize_t size = read_port(self->fPort, &code, &key, sizeof(key));
		if (size == B_INTERRUPTED)
			continue;
		if (size < 0)
			break;
		// Anyone may write to the port: only take whole, terminated keys.
		if (code != kVirtualKeyCode || size != sizeof(key)
			|| memchr(key.bytes, '\0', sizeof(key.bytes)) == NULL) {
			continue;
		}
		self->_Type(key);
	}
	return B_OK;
}


void
VirtualKeyboardDevice::_Modifiers(uint32 from, uint32 to)
{
	BMessage* message = new BMessage(B_MODIFIERS_CHANGED);
	message->AddInt64("when", system_time());
	message->AddInt32("be:old_modifiers", from);
	message->AddInt32("modifiers", to);
	if (EnqueueMessage(message) != B_OK)
		delete message;
}


void
VirtualKeyboardDevice::_Type(const virtual_key& key)
{
	// input_server takes the modifiers every app sees from the last key
	// event, so they're pressed and let go around the key, as on a real
	// keyboard: otherwise they would stay down until the next key.
	if (key.modifiers != 0)
		_Modifiers(0, key.modifiers);

	for (int down = 1; down >= 0; down--) {
		BMessage* message = new BMessage(down ? B_KEY_DOWN : B_KEY_UP);
		message->AddInt64("when", system_time());
		message->AddInt32("key", key.key);
		message->AddInt32("modifiers", key.modifiers);
		int32 length = strlen(key.bytes);
		for (int32 i = 0; i < length; i++)
			message->AddInt8("byte", (int8)key.bytes[i]);
		message->AddData("bytes", B_STRING_TYPE, key.bytes, length + 1);
		message->AddInt32("raw_char", (uint8)key.raw & 0x7f);
		if (EnqueueMessage(message) != B_OK)
			delete message;
	}

	if (key.modifiers != 0)
		_Modifiers(key.modifiers, 0);
}


extern "C" BInputServerDevice*
instantiate_input_device()
{
	return new VirtualKeyboardDevice();
}
