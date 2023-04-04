
#include "san_activity.hpp"

class app : public san::android::activity_base {
public:
	app() {} // Main (GUI) thread!
	virtual ~app() {}

	void on_start() override {}
	void on_stop() override {}
	void on_redraw() override { on_idle(); }

	void on_window_init( ANativeWindow * p_window ) override {
		ANativeWindow_setBuffersGeometry( p_window, 0, 0, WINDOW_FORMAT_RGBX_8888 );
	}

	void on_window_term() override {}

	bool on_input_event( const AInputEvent * p_event ) override {
		quit();
		return true; // Is event handled?
	}

	void on_idle() override {
		ANativeWindow_Buffer buffer;
		if ( ANativeWindow_lock( get_window(), &buffer, nullptr ) < 0 ) {
			LOG( "Unable to lock window buffer" );
			return;
		}

		static bool is_first_frame = false;
		if ( !is_first_frame ) {
			LOG( "w = %d, h = %d, s = %d\n", buffer.width, buffer.height, buffer.stride );
			is_first_frame = true;
		}

		static uint32_t c = 0;
		for ( int i = 0; i < buffer.stride * buffer.height; ++i ) {
			static_cast<uint32_t *>( buffer.bits )[i] = c;
		}
		c += 0x01020304;

		ANativeWindow_unlockAndPost( get_window() );
	}
};

namespace san::android {

activity_base * main( ANativeActivity * p_activity, void * p_preempted_state, size_t zu_preempted_state_size ) {
	static app a;
	return &a;
}

} // namespace san::android
