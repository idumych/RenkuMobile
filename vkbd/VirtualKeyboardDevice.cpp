/*
 * Renku virtual keyboard: an input_server device add-on that types what the
 * shell's on-screen keyboard sends to its port, as if a real keyboard had.
 */
#include "VirtualKeyboard.h"

#include <InputServerDevice.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <OS.h>

#include <string.h>


class VirtualKeyboardDevice : public BInputServerDevice {
public:
								VirtualKeyboardDevice();

	virtual	status_t			InitCheck();
	virtual	status_t			Start(const char* device, void* cookie);
	virtual	status_t			Stop(const char* device, void* cookie);
	virtual	status_t			Control(const char* device, void* cookie,
									uint32 code, BMessage* message);

private:
	static	status_t			_Reader(void* data);
			void				_Type(const virtual_key& key);

			port_id				fPort;
			thread_id			fThread;
};


VirtualKeyboardDevice::VirtualKeyboardDevice()
	:
	fPort(-1),
	fThread(-1)
{
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
VirtualKeyboardDevice::_Type(const virtual_key& key)
{
	for (int down = 1; down >= 0; down--) {
		BMessage* message = new BMessage(down ? B_KEY_DOWN : B_KEY_UP);
		message->AddInt64("when", system_time());
		message->AddInt32("key", key.key);
		message->AddInt32("modifiers", key.modifiers);
		int32 length = strlen(key.bytes);
		for (int32 i = 0; i < length; i++)
			message->AddInt8("byte", (int8)key.bytes[i]);
		message->AddData("bytes", B_STRING_TYPE, key.bytes, length + 1);
		message->AddInt32("raw_char", (uint8)key.bytes[0] & 0x7f);
		if (EnqueueMessage(message) != B_OK)
			delete message;
	}
}


extern "C" BInputServerDevice*
instantiate_input_device()
{
	return new VirtualKeyboardDevice();
}
