/*
	Skelton for retropc emulator

	[ host frame callback ]
*/

#ifndef _HOST_FRAME_CALLBACK_H_
#define _HOST_FRAME_CALLBACK_H_

class HostFrameCallback
{
public:
	typedef void (*Callback)(void *userdata);

	HostFrameCallback() : callback_(nullptr), userdata_(nullptr) {}

	void set(Callback callback, void *userdata)
	{
		callback_ = callback;
		userdata_ = userdata;
	}

	void notify() const
	{
		if(callback_ != nullptr) {
			callback_(userdata_);
		}
	}

private:
	Callback callback_;
	void *userdata_;
};

#endif // _HOST_FRAME_CALLBACK_H_
