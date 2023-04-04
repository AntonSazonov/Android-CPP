
// TODO: rename 'saved state' to 'preempted state'

#pragma once

#include <cstdio>
//#include <memory>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <thread>

#include <jni.h>
#include <android/log.h>
#include <android/native_activity.h>

#define LOG_TAG "san"

#ifndef NDEBUG
 #define LOG( ... ) __android_log_print( ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__ )
 #define ASSERT( cond, ... ) if ( !(cond) ) { __android_log_assert( #cond, LOG_TAG, __VA_ARGS__ ); }
#else
 #define LOG( ... )
 #define ASSERT( cond, ... )
#endif

inline void LOG_LONG( const char * str ) {
	for ( ; ; ) {
		const char * p = std::strstr( str, "\n" );
		if ( !p ) break;
		int len = p - str;
		__android_log_print( ANDROID_LOG_INFO, LOG_TAG, "%.*s", len, str );
		str += len + 1;
	}   
	__android_log_print( ANDROID_LOG_INFO, LOG_TAG, "%s", str );
}

namespace san::android {

#define LOGF()		LOG( "[%s]\n", __PRETTY_FUNCTION__ )
#define LOGE( msg )	LOG( "[%s, %s, %4d]: %s\n", __FILE__, __FUNCTION__, __LINE__, (msg) )

class noncopyable {
	noncopyable( const noncopyable & ) = delete;
	const noncopyable & operator = ( const noncopyable & ) = delete;

protected:
	noncopyable() = default;
}; // class noncopyable


// Pipe RAII wrapper
class pipe : noncopyable {
	bool	m_is_valid;
	int		m_pipe[2];

public:
	pipe( int flags = 0 )
		: m_is_valid( !::pipe2( m_pipe, flags ) ) {}

	virtual ~pipe() {
		if ( m_is_valid ) {
			::close( m_pipe[0] );
			::close( m_pipe[1] );
		}
	}

	explicit operator bool () const noexcept { return m_is_valid; }

	int get_read_fd() const noexcept { return m_pipe[0]; }
	int get_write_fd() const noexcept { return m_pipe[1]; }

	template <typename T>
	bool read( T & value ) const noexcept {
		return m_is_valid && ::read( m_pipe[0], &value, sizeof( value ) ) == sizeof( value );
	}

	template <typename T>
	bool write( const T & value ) const noexcept {
		return m_is_valid && ::write( m_pipe[1], &value, sizeof( value ) ) == sizeof( value );
	}
}; // class pipe





class activity_base : noncopyable {
	friend void ::ANativeActivity_onCreate( ANativeActivity *, void *, size_t );

	enum command_t : int8_t {
		e_set_input_queue	= 1,
		e_set_window,
		e_on_start,
		e_on_stop,
		e_on_redraw
	};

	bool send_command( int8_t cmd ) {
		if ( !m_pipe.write<int8_t>( cmd ) ) {
			LOG( "Pipe write failed: %s", std::strerror( errno ) );
			quit();
			return false;
		}
		return true;
	}

	static void onConfigurationChanged( ANativeActivity * p_activity ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		//LOGF();
	}

	static void onContentRectChanged( ANativeActivity * p_activity, const ARect * p_rect ) { LOGF(); }
	static void onDestroy( ANativeActivity * p_activity ) { LOGF(); }


	// Input queue
	void set_input_queue( AInputQueue * p_queue ) {
		std::unique_lock lock( m_mutex );
		m_input_queue_pending = p_queue;
		if ( !send_command( command_t::e_set_input_queue ) ) return;
		m_cv.wait( lock, [&]{ return m_input_queue == m_input_queue_pending; } );
	}

	static void onInputQueueCreated( ANativeActivity * p_activity, AInputQueue * p_queue ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		//LOG( "[%s]: p_queue = %p\n", __PRETTY_FUNCTION__, p_queue );
		p_this->set_input_queue( p_queue );
	}

	static void onInputQueueDestroyed( ANativeActivity * p_activity, AInputQueue * p_queue ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		//LOG( "[%s]: p_queue = %p\n", __PRETTY_FUNCTION__, p_queue );
		p_this->set_input_queue( nullptr );
	}


	static void onLowMemory( ANativeActivity * p_activity ) { LOGF(); }


	// Window
	void set_window( ANativeWindow * p_window ) {
		std::unique_lock lock( m_mutex );
		m_window_pending = p_window;
		if ( !send_command( command_t::e_set_window ) ) return;
		m_cv.wait( lock, [&]{ return m_window == m_window_pending; } );
	}

	static void onNativeWindowCreated( ANativeActivity * p_activity, ANativeWindow * p_window ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		//LOG( "[%s]: p_window = %p\n", __PRETTY_FUNCTION__, p_window );
		ANativeWindow_acquire( p_window );
		p_this->set_window( p_window );
	}

	static void onNativeWindowDestroyed( ANativeActivity * p_activity, ANativeWindow * p_window ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		//LOG( "[%s]: p_window = %p\n", __PRETTY_FUNCTION__, p_window );
		p_this->set_window( nullptr );
		ANativeWindow_release( p_window );
	}

	static void onNativeWindowRedrawNeeded( ANativeActivity * p_activity, ANativeWindow * p_window ) {
		LOGF();
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		p_this->send_command( command_t::e_on_redraw );
	}


	static void onNativeWindowResized( ANativeActivity * p_activity, ANativeWindow * p_window ) { LOGF(); }
	static void onPause( ANativeActivity * p_activity ) { LOGF(); }
	static void onResume( ANativeActivity * p_activity ) { LOGF(); }
	static void *onSaveInstanceState( ANativeActivity * p_activity, size_t * p_outSize ) { LOGF(); return nullptr; }


	static void onStart( ANativeActivity * p_activity ) {
		//LOGF();
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		p_this->send_command( command_t::e_on_start );
	}

	static void onStop( ANativeActivity * p_activity ) {
		//LOGF();
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_activity->instance );
		ASSERT( p_this, "!p_this" );
		p_this->send_command( command_t::e_on_stop );
	}


	static void onWindowFocusChanged( ANativeActivity * p_activity, int hasFocus ) { LOGF(); }


	bool					m_is_valid				= false;
	bool					m_is_user_thread_ready	= false;
	bool					m_is_window_ready		= false;
	bool					m_is_running			= true;
	bool					m_is_animating			= true;	// don't wait for events

	std::mutex				m_mutex;
	std::condition_variable	m_cv;
	pipe					m_pipe;

	ANativeActivity *		m_activity				= nullptr;
	ALooper *				m_looper				= nullptr;
	AInputQueue *			m_input_queue			= nullptr;
	AInputQueue *			m_input_queue_pending	= nullptr;
	ANativeWindow *			m_window				= nullptr;
	ANativeWindow *			m_window_pending		= nullptr;

	// Called from user thread
	static int process_input( int fd, int events, void * p_param ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_param );
		ASSERT( p_this, "!p_this" );
		//LOG( "[%s]: p_this = %p\n", __PRETTY_FUNCTION__, p_this );

		AInputEvent * p_event = nullptr;
		while ( AInputQueue_getEvent( p_this->m_input_queue, &p_event ) >= 0 ) {

			// Sends the key for standard pre-dispatching that is, possibly deliver it to the current IME to be consumed before the app.
			// Returns 0 if it was not pre-dispatched, meaning you can process it right now. If non-zero is returned,
			//  you must abandon the current event processing and allow the event to appear again in the event queue (if it does not get consumed during pre-dispatching).
			if ( AInputQueue_preDispatchEvent( p_this->m_input_queue, p_event ) ) continue;

			// Report that dispatching has finished with the given event.
			// This must be called after receiving an event with AInputQueue_get_event().
			AInputQueue_finishEvent( p_this->m_input_queue, p_event, p_this->on_input_event( p_event ) );
		}

		return 1; // 1 to continue receiving callbacks, or 0 to have this file descriptor and callback unregistered from the looper.
	}


	template <typename T>
	void set_atomic_var( T & var_to_set, const T & new_value ) {
		std::unique_lock var_lock( m_mutex );
		var_to_set = new_value;
		m_cv.notify_all();
	}

	// Called from user thread
	static int process_cmd( int /*fd*/, int /*events*/, void * p_param ) {
		san::android::activity_base * p_this = static_cast<san::android::activity_base *>( p_param );
		ASSERT( p_this, "!p_this" );

		command_t r_cmd;
		if ( !p_this->m_pipe.read( r_cmd ) ) {
			LOG( "Pipe read failed: %s", std::strerror( errno ) );
			p_this->quit();
			return 0;
		}

		//LOG( "[%s]: p_this = %p, r_cmd = %d\n", __PRETTY_FUNCTION__, p_this, r_cmd );

		switch ( r_cmd ) {
			case command_t::e_set_input_queue: {
				if ( p_this->m_input_queue ) {
					AInputQueue_detachLooper( p_this->m_input_queue );
				}

				p_this->set_atomic_var( p_this->m_input_queue, p_this->m_input_queue_pending );
#if 0
				{
					std::unique_lock lock( p_this->m_mutex );
					p_this->m_input_queue = p_this->m_input_queue_pending;
					p_this->m_cv.notify_all();
				}
#endif

				if ( p_this->m_input_queue ) {
					AInputQueue_attachLooper( p_this->m_input_queue, p_this->m_looper, ALOOPER_POLL_CALLBACK, process_input, p_this );
				}
			} break;



			case command_t::e_set_window: {
				p_this->set_atomic_var( p_this->m_window, p_this->m_window_pending );
#if 0
				{
					std::unique_lock lock( p_this->m_mutex );
					p_this->m_window = p_this->m_window_pending;
					p_this->m_cv.notify_all();
				}
#endif

				if ( p_this->m_window ) {
					p_this->on_window_init( p_this->m_window );
					p_this->m_is_window_ready = true;
				} else {
					p_this->m_is_window_ready = false;
					p_this->on_window_term();
				}
			} break;

			case command_t::e_on_start:		p_this->on_start();		break;
			case command_t::e_on_stop:		p_this->on_stop();		break;
			case command_t::e_on_redraw:	p_this->on_redraw();	break;

			default:
				LOG( "[%s]: unknown command %d\n", __PRETTY_FUNCTION__, r_cmd );
				break;
		}

		return 1; // 1 to continue receiving callbacks, or 0 to have this file descriptor and callback unregistered from the looper.
	}

public:
	activity_base() {
		//LOG( "[%s] - this = %p\n", __PRETTY_FUNCTION__, this );
		if ( !m_pipe ) {
			LOG( "Could not create pipe: %s", std::strerror( errno ) );
			quit();
			return;
		}

		std::thread user_thread( [&]{
				// TODO: add error checking
				m_looper = ALooper_prepare( 0 );
				ALooper_addFd( m_looper, m_pipe.get_read_fd(), ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, process_cmd, this );

				m_is_user_thread_ready = true; // set it to ready after prepare the Looper
				m_cv.notify_all();

				//LOG( "[%s]: Enter user loop.\n", __PRETTY_FUNCTION__ );
				for ( ; m_is_running; ) {
					int ident = ALooper_pollOnce( m_is_animating ? 0 : -1, nullptr, nullptr, nullptr ); // If the timeout is zero, returns immediately without blocking. If the timeout is negative, waits indefinitely until an event appears.
					//int r = ALooper_pollOnce( -1, nullptr, nullptr, nullptr );
					//LOG( "[%s]: r = %d\n", __PRETTY_FUNCTION__, r );

					//static int i = 0;
					//if ( i < 3 ) LOG( "[%s]: %d, m_is_animating = %d\n", __PRETTY_FUNCTION__, i++, int(m_is_animating) );
					if ( m_is_window_ready ) on_idle();
				}

				//LOG( "[%s]: Leave user loop.\n", __PRETTY_FUNCTION__ );
			} );

		//LOG( "[%s]: ---  pre wait for user thread ready...\n", __PRETTY_FUNCTION__ );
		{ // Wait for user to ready...
			std::unique_lock lock( m_mutex );
			m_cv.wait( lock, [&]{ return m_is_user_thread_ready; } );
		}
		user_thread.detach();
		//LOG( "[%s]: --- post wait for user thread ready...\n", __PRETTY_FUNCTION__ );

		m_is_valid = true;
	}

	virtual ~activity_base() {}

	explicit operator bool () const noexcept { return m_is_valid; }

	void quit() {
		//LOG( "[%s]\n", __PRETTY_FUNCTION__ );
		m_is_running = false;
		ANativeActivity_finish( m_activity );
	}

	void set_animating( bool animating ) {
		m_is_animating = animating;
	}

	ANativeWindow * get_window() const { return m_window; }

	virtual void on_start() {}
	virtual void on_stop() {}
	virtual void on_redraw() {}

	virtual void on_window_init( ANativeWindow * ) = 0;
	virtual void on_window_term() = 0;

	virtual bool on_input_event( const AInputEvent * ) = 0;
	virtual void on_idle() = 0;
}; // class activity_base : noncopyable

san::android::activity_base * main( ANativeActivity *, void *, size_t );

} // namespace san::android

// https://stackoverflow.com/questions/47507714/how-do-i-enable-full-screen-immersive-mode-for-a-native-activity-ndk-app
static void set_immersive_mode( ANativeActivity * p_activity ) {
	JNIEnv * p_env = p_activity->env;

	jclass activityClass = p_env->FindClass( "android/app/NativeActivity" );
	jclass windowClass = p_env->FindClass( "android/view/Window" );
	jclass viewClass = p_env->FindClass( "android/view/View" );

	jmethodID getWindow = p_env->GetMethodID( activityClass, "getWindow", "()Landroid/view/Window;" );
	jmethodID getDecorView = p_env->GetMethodID( windowClass, "getDecorView", "()Landroid/view/View;" );
	jmethodID setSystemUiVisibility = p_env->GetMethodID( viewClass, "setSystemUiVisibility", "(I)V" );

	jobject windowObj = p_env->CallObjectMethod( p_activity->clazz, getWindow );
	jobject decorViewObj = p_env->CallObjectMethod( windowObj, getDecorView );

	// https://developer.android.com/reference/android/view/View
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_STABLE = p_env->GetStaticFieldID( viewClass, "SYSTEM_UI_FLAG_LAYOUT_STABLE", "I" );
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = p_env->GetStaticFieldID( viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I" );
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = p_env->GetStaticFieldID( viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I" );
	jfieldID id_SYSTEM_UI_FLAG_HIDE_NAVIGATION = p_env->GetStaticFieldID( viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I" );
	jfieldID id_SYSTEM_UI_FLAG_FULLSCREEN = p_env->GetStaticFieldID( viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I" );
	jfieldID id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = p_env->GetStaticFieldID( viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I" );

	// Get flags
	int flags = 0;
	flags |= p_env->GetStaticIntField( viewClass, id_SYSTEM_UI_FLAG_LAYOUT_STABLE );
	flags |= p_env->GetStaticIntField( viewClass, id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION );
	flags |= p_env->GetStaticIntField( viewClass, id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN );
	flags |= p_env->GetStaticIntField( viewClass, id_SYSTEM_UI_FLAG_HIDE_NAVIGATION );
	flags |= p_env->GetStaticIntField( viewClass, id_SYSTEM_UI_FLAG_FULLSCREEN );
	flags |= p_env->GetStaticIntField( viewClass, id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY );

	p_env->CallVoidMethod( decorViewObj, setSystemUiVisibility, flags );

	if ( p_env->ExceptionCheck() ) {
		jthrowable e = p_env->ExceptionOccurred();
		p_env->ExceptionClear();
		jclass clazz = p_env->GetObjectClass( e );
		jmethodID getMessage = p_env->GetMethodID( clazz, "getMessage", "()Ljava/lang/String;" );
		jstring message = (jstring)p_env->CallObjectMethod( e, getMessage );

		const char * mstr = p_env->GetStringUTFChars( message, NULL );
		LOG( "Immersive mode exception:\n  %s", mstr );
		LOG( "\n" );
		p_env->ReleaseStringUTFChars( message, mstr );

		p_env->DeleteLocalRef( message );
		p_env->DeleteLocalRef( clazz );
		p_env->DeleteLocalRef( e );
	} else {
		LOG( "Immersive mode enabled.\n" );
	}
	p_env->DeleteLocalRef( windowObj );
	p_env->DeleteLocalRef( decorViewObj );
}

JNIEXPORT void ANativeActivity_onCreate( ANativeActivity * p_activity, void * p_preempted_state, size_t zu_preempted_state_size ) {
	LOG( "[%s]: p_preempted_state = %p, zu_preempted_state_size = %zu\n", __PRETTY_FUNCTION__, p_preempted_state, zu_preempted_state_size );
	set_immersive_mode( p_activity );

	p_activity->callbacks->onConfigurationChanged		= san::android::activity_base::onConfigurationChanged;
	p_activity->callbacks->onContentRectChanged			= san::android::activity_base::onContentRectChanged;
	p_activity->callbacks->onDestroy					= san::android::activity_base::onDestroy;
	p_activity->callbacks->onInputQueueCreated			= san::android::activity_base::onInputQueueCreated;
	p_activity->callbacks->onInputQueueDestroyed		= san::android::activity_base::onInputQueueDestroyed;
	p_activity->callbacks->onLowMemory					= san::android::activity_base::onLowMemory;
	p_activity->callbacks->onNativeWindowCreated		= san::android::activity_base::onNativeWindowCreated;
	p_activity->callbacks->onNativeWindowDestroyed		= san::android::activity_base::onNativeWindowDestroyed;
	p_activity->callbacks->onNativeWindowRedrawNeeded	= san::android::activity_base::onNativeWindowRedrawNeeded;
	p_activity->callbacks->onNativeWindowResized		= san::android::activity_base::onNativeWindowResized;
	p_activity->callbacks->onPause						= san::android::activity_base::onPause;
	p_activity->callbacks->onResume						= san::android::activity_base::onResume;
	p_activity->callbacks->onSaveInstanceState			= san::android::activity_base::onSaveInstanceState;
	p_activity->callbacks->onStart						= san::android::activity_base::onStart;
	p_activity->callbacks->onStop						= san::android::activity_base::onStop;
	p_activity->callbacks->onWindowFocusChanged			= san::android::activity_base::onWindowFocusChanged;

	san::android::activity_base * p_act = san::android::main( p_activity, p_preempted_state, zu_preempted_state_size );
	p_activity->instance = p_act;
	p_act->m_activity = p_activity;
}
