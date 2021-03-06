// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "WindowSDL2.h"
#include "framework/Vulkan/VulkanSurface.h"
#include "stl/Containers/Singleton.h"

#ifdef FG_ENABLE_SDL2
#	include "SDL_syswm.h"

namespace FGC
{
namespace {
	struct SDL2Instance
	{
		uint	refCounter	= 0;
		bool	initialized	= false;
	};
}

/*
=================================================
	constructor
=================================================
*/
	WindowSDL2::WindowSDL2 () :
		_window{ null },
		_wndID{ 0 }
	{
		for (auto& state : _keyStates) {
			state = EKeyAction(~0u);
		}
	}
	
/*
=================================================
	destructor
=================================================
*/
	WindowSDL2::~WindowSDL2 ()
	{
		Destroy();
	}
	
/*
=================================================
	Create
=================================================
*/
	bool WindowSDL2::Create (uint2 surfaceSize, NtStringView title)
	{
		CHECK_ERR( not _window );

		auto&	inst = *Singleton<SDL2Instance>();

		if ( not inst.initialized )
		{
			CHECK( SDL_Init( SDL_INIT_EVERYTHING ) == 0 );
			inst.initialized = true;
		}

		++inst.refCounter;

		//const int	count	= SDL_GetNumVideoDisplays();
		//const int	disp_idx= 0;

		//SDL_Rect	area	= {};
		//CHECK( SDL_GetDisplayUsableBounds( disp_idx, OUT &area ));

		//const int2	pos		= int2(area.x + area.w/2 - surfaceSize.x/2, area.y + area.h/2 - surfaceSize.y/2);

		const uint	flags	= int(SDL_WINDOW_ALLOW_HIGHDPI) | int(SDL_WINDOW_RESIZABLE);

		CHECK_ERR( (_window = SDL_CreateWindow( title.c_str(),
												SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
												surfaceSize.x, surfaceSize.y,
												flags )) != null );

		_wndID = SDL_GetWindowID( _window );

		SDL_SetWindowData( _window, "fg", this );
		
		return true;
	}
	
/*
=================================================
	AddListener
=================================================
*/
	void WindowSDL2::AddListener (IWindowEventListener *listener)
	{
		ASSERT( listener );
		_listeners.insert( listener );
	}
	
/*
=================================================
	RemoveListener
=================================================
*/
	void WindowSDL2::RemoveListener (IWindowEventListener *listener)
	{
		ASSERT( listener );
		_listeners.erase( listener );
	}

/*
=================================================
	Update
=================================================
*/
	bool WindowSDL2::Update ()
	{
		const auto	OnKeyEvent = [this] (bool down, SDL_Scancode scancode)
		{
			auto&	state = _keyStates[ scancode ];

			if ( down ) {
				if ( state == EKeyAction::Down or state == EKeyAction::Pressed )
					state = EKeyAction::Pressed;
				else
					state = EKeyAction::Down;
			}else
				state = EKeyAction::Up;
			
			for (auto& active : _activeKeys)
			{
				// skip duplicates
				if ( active.first == scancode )
				{
					if ( state == EKeyAction::Up )
						active.second = state;

					return;
				}
			}

			_activeKeys.push_back({ scancode, state });
		};
		//---------------------------------------------------------------------------


		if ( not _window )
			return false;

		SDL_Event	ev;
		while ( SDL_PollEvent( OUT &ev ))
		{
			// check for events from different window
			bool	other_wnd = false;

			switch ( ev.type )
			{
				case SDL_WINDOWEVENT :		other_wnd = (ev.window.windowID != _wndID);	break;
				case SDL_KEYDOWN :
				case SDL_KEYUP :			other_wnd = (ev.key.windowID != _wndID);	break;
				case SDL_TEXTEDITING :		other_wnd = (ev.edit.windowID != _wndID);	break;
				case SDL_TEXTINPUT :		other_wnd = (ev.text.windowID != _wndID);	break;
				case SDL_MOUSEMOTION :		other_wnd = (ev.motion.windowID != _wndID);	break;
				case SDL_MOUSEBUTTONDOWN :
				case SDL_MOUSEBUTTONUP :	other_wnd = (ev.button.windowID != _wndID);	break;
				case SDL_MOUSEWHEEL :		other_wnd = (ev.wheel.windowID != _wndID);	break;
			}

			if ( other_wnd )
			{
				SDL_PushEvent( &ev );
				return true;
			}

			switch ( ev.type )
			{
				case SDL_QUIT :
				case SDL_APP_TERMINATING :
					Quit();
					return false;

				case SDL_APP_WILLENTERBACKGROUND :
					break;

				case SDL_APP_DIDENTERBACKGROUND :
					break;

				case SDL_APP_WILLENTERFOREGROUND :
					break;

				case SDL_APP_DIDENTERFOREGROUND :
					break;
					
				case SDL_MOUSEMOTION :
				{
					float2	pos  = { float(ev.motion.x), float(ev.motion.y) };

					for (auto& listener : _listeners) {
						listener->OnMouseMove( pos );
					}
					break;
				}

				case SDL_MOUSEBUTTONDOWN :
				case SDL_MOUSEBUTTONUP :
				{
					OnKeyEvent( ev.type == SDL_MOUSEBUTTONDOWN, SDL_Scancode(SDL_NUM_SCANCODES + ev.button.button) );
					break;
				}

				case SDL_MOUSEWHEEL :
				{
					break;
				}

				case SDL_KEYDOWN :
				case SDL_KEYUP :
				{
					OnKeyEvent( ev.type == SDL_KEYDOWN, ev.key.keysym.scancode );
					break;
				}

				case SDL_WINDOWEVENT :
				{
					switch ( ev.window.event )
					{
						case SDL_WINDOWEVENT_SHOWN :
						{
							break;
						}

						case SDL_WINDOWEVENT_HIDDEN :
						{
							break;
						}

						case SDL_WINDOWEVENT_RESIZED :
						//case SDL_WINDOWEVENT_MOVED :
						//case SDL_WINDOWEVENT_SIZE_CHANGED :
						{
							if ( _window )
							{
								int2	size;
								SDL_GetWindowSize( _window, OUT &size.x, OUT &size.y );
							
								for (auto& listener : _listeners) {
									listener->OnResize( uint2(size) );
								}
							}
							break;
						}

						case SDL_WINDOWEVENT_CLOSE :
						{
							Quit();
							return false;
						}
					}
					break;
				}
			}
		}
		
		for (auto key_iter = _activeKeys.begin(); key_iter != _activeKeys.end();)
		{
			StringView	key_name	= _MapKey( key_iter->first );
			EKeyAction&	action		= key_iter->second;
			
			if ( key_name.size() )
			{
				for (auto& listener : _listeners) {
					listener->OnKey( key_name, action );
				}
			}
			
			BEGIN_ENUM_CHECKS();
			switch ( action ) {
				case EKeyAction::Up :		key_iter = _activeKeys.erase( key_iter );	break;
				case EKeyAction::Down :		action = EKeyAction::Pressed;				break;
				case EKeyAction::Pressed :	++key_iter;									break;
			}
			END_ENUM_CHECKS();
		}
		
		if ( not _window )
			return false;

		for (auto& listener : _listeners) {
			listener->OnUpdate();
		}

		return true;
	}
	
/*
=================================================
	Quit
=================================================
*/
	void WindowSDL2::Quit ()
	{
		Destroy();
	}
	
/*
=================================================
	Destroy
=================================================
*/
	void WindowSDL2::Destroy ()
	{
		for (auto& listener : _listeners) {
			listener->OnDestroy();
		}

		if ( _window )
		{
			SDL_DestroyWindow( _window );
			_window = null;
			_wndID = 0;
		
			auto&	inst = *Singleton<SDL2Instance>();

			if ( --inst.refCounter == 0 and inst.initialized )
			{
				SDL_Quit();
				inst.initialized = false;
			}
		}
	}
	
/*
=================================================
	SetTitle
=================================================
*/
	void WindowSDL2::SetTitle (NtStringView value)
	{
		CHECK_ERRV( _window );

		SDL_SetWindowTitle( _window, value.c_str() );
	}
	
/*
=================================================
	SetSize
=================================================
*/
	void WindowSDL2::SetSize (const uint2 &value)
	{
		CHECK_ERRV( _window );

		SDL_SetWindowSize( _window, int(value.x), int(value.y) );
	}
	
/*
=================================================
	SetPosition
=================================================
*/
	void WindowSDL2::SetPosition (const int2 &value)
	{
		CHECK_ERRV( _window );

		SDL_SetWindowPosition( _window, value.x, value.y );
	}

/*
=================================================
	GetSize
=================================================
*/
	uint2  WindowSDL2::GetSize () const
	{
		CHECK_ERR( _window );

		int2	size;
		SDL_GetWindowSize( _window, OUT &size.x, OUT &size.y );

		return uint2(size);
	}
	
/*
=================================================
	_MapKey
=================================================
*/
	StringView  WindowSDL2::_MapKey (SDL_Scancode code)
	{
		switch ( int(code) )
		{
			case SDL_SCANCODE_BACKSPACE		: return "backspace";
			case SDL_SCANCODE_TAB			: return "tab";
			case SDL_SCANCODE_CLEAR			: return "clear";
			case SDL_SCANCODE_RETURN		: return "enter";
			case SDL_SCANCODE_LCTRL			: return "l-ctrl";
			case SDL_SCANCODE_RCTRL			: return "r-ctrl";
			case SDL_SCANCODE_LALT			: return "l-alt";
			case SDL_SCANCODE_RALT			: return "r-alt";
			case SDL_SCANCODE_PAUSE			: return "pause";
			case SDL_SCANCODE_CAPSLOCK		: return "caps lock";
			case SDL_SCANCODE_ESCAPE		: return "escape";
			case SDL_SCANCODE_SPACE			: return "space";
			case SDL_SCANCODE_PAGEUP		: return "page up";
			case SDL_SCANCODE_PAGEDOWN		: return "page down";
			case SDL_SCANCODE_END			: return "end";
			case SDL_SCANCODE_HOME			: return "home";
			case SDL_SCANCODE_LEFT			: return "arrow left";
			case SDL_SCANCODE_UP			: return "arrow up";
			case SDL_SCANCODE_RIGHT			: return "arrow right";
			case SDL_SCANCODE_DOWN			: return "arrow down";
			case SDL_SCANCODE_PRINTSCREEN	: return "print screen";
			case SDL_SCANCODE_INSERT		: return "insert";
			case SDL_SCANCODE_DELETE		: return "delete";

			case SDL_SCANCODE_0				: return "0";
			case SDL_SCANCODE_1				: return "1";
			case SDL_SCANCODE_2				: return "2";
			case SDL_SCANCODE_3				: return "3";
			case SDL_SCANCODE_4				: return "4";
			case SDL_SCANCODE_5				: return "5";
			case SDL_SCANCODE_6				: return "6";
			case SDL_SCANCODE_7				: return "7";
			case SDL_SCANCODE_8				: return "8";
			case SDL_SCANCODE_9				: return "9";

			case SDL_SCANCODE_A				: return "A";
			case SDL_SCANCODE_B				: return "B";
			case SDL_SCANCODE_C				: return "C";
			case SDL_SCANCODE_D				: return "D";
			case SDL_SCANCODE_E				: return "E";
			case SDL_SCANCODE_F				: return "F";
			case SDL_SCANCODE_G				: return "G";
			case SDL_SCANCODE_H				: return "H";
			case SDL_SCANCODE_I				: return "I";
			case SDL_SCANCODE_J				: return "J";
			case SDL_SCANCODE_K				: return "K";
			case SDL_SCANCODE_L				: return "L";
			case SDL_SCANCODE_M				: return "M";
			case SDL_SCANCODE_N				: return "N";
			case SDL_SCANCODE_O				: return "O";
			case SDL_SCANCODE_P				: return "P";
			case SDL_SCANCODE_Q				: return "Q";
			case SDL_SCANCODE_R				: return "R";
			case SDL_SCANCODE_S				: return "S";
			case SDL_SCANCODE_T				: return "T";
			case SDL_SCANCODE_U				: return "U";
			case SDL_SCANCODE_V				: return "V";
			case SDL_SCANCODE_W				: return "W";
			case SDL_SCANCODE_X				: return "X";
			case SDL_SCANCODE_Y				: return "Y";
			case SDL_SCANCODE_Z				: return "Z";
				
			case SDL_SCANCODE_KP_ENTER		: return "numpad enter";
			case SDL_SCANCODE_KP_0			: return "numpad 0";
			case SDL_SCANCODE_KP_1			: return "numpad 1";
			case SDL_SCANCODE_KP_2			: return "numpad 2";
			case SDL_SCANCODE_KP_3			: return "numpad 3";
			case SDL_SCANCODE_KP_4			: return "numpad 4";
			case SDL_SCANCODE_KP_5			: return "numpad 5";
			case SDL_SCANCODE_KP_6			: return "numpad 6";
			case SDL_SCANCODE_KP_7			: return "numpad 7";
			case SDL_SCANCODE_KP_8			: return "numpad 8";
			case SDL_SCANCODE_KP_9			: return "numpad 9";
			case SDL_SCANCODE_KP_MULTIPLY	: return "numpad *";
			case SDL_SCANCODE_KP_PLUS		: return "numpad +";
			case SDL_SCANCODE_SEPARATOR		: return "numpad sep";
			case SDL_SCANCODE_KP_MINUS		: return "numpad -";
			case SDL_SCANCODE_KP_PERIOD		: return "numpad .";
			case SDL_SCANCODE_KP_DIVIDE		: return "numpad /";
			case SDL_SCANCODE_KP_EQUALS		: return "numpad =";
			case SDL_SCANCODE_KP_COMMA		: return "numpad ,";

			case SDL_SCANCODE_F1			: return "F1";
			case SDL_SCANCODE_F2			: return "F2";
			case SDL_SCANCODE_F3			: return "F3";
			case SDL_SCANCODE_F4			: return "F4";
			case SDL_SCANCODE_F5			: return "F5";
			case SDL_SCANCODE_F6			: return "F6";
			case SDL_SCANCODE_F7			: return "F7";
			case SDL_SCANCODE_F8			: return "F8";
			case SDL_SCANCODE_F9			: return "F9";
			case SDL_SCANCODE_F10			: return "F10";
			case SDL_SCANCODE_F11			: return "F11";
			case SDL_SCANCODE_F12			: return "F12";
			
			case SDL_SCANCODE_NUMLOCKCLEAR	: return "num lock";
			case SDL_SCANCODE_SCROLLLOCK	: return "scroll lock";

			case SDL_SCANCODE_SEMICOLON		: return ";";
			case SDL_SCANCODE_EQUALS		: return "=";
			case SDL_SCANCODE_COMMA			: return ",";
			case SDL_SCANCODE_MINUS			: return "-";
			case SDL_SCANCODE_PERIOD		: return ".";
			case SDL_SCANCODE_BACKSLASH		: return "/";
			case SDL_SCANCODE_GRAVE			: return "~";
			case SDL_SCANCODE_LEFTBRACKET	: return "[";
			case SDL_SCANCODE_SLASH			: return "\\";
			case SDL_SCANCODE_RIGHTBRACKET	: return "]";
			case SDL_SCANCODE_APOSTROPHE	: return "'";

			case SDL_NUM_SCANCODES + 1		: return "left mb";
			case SDL_NUM_SCANCODES + 2		: return "right mb";
			case SDL_NUM_SCANCODES + 3		: return "middle mb";
			case SDL_NUM_SCANCODES + 4		: return "mouse btn 4";
			case SDL_NUM_SCANCODES + 5		: return "mouse btn 5";
			case SDL_NUM_SCANCODES + 6		: return "mouse btn 6";
			case SDL_NUM_SCANCODES + 7		: return "mouse btn 7";
			case SDL_NUM_SCANCODES + 8		: return "mouse btn 8";
		}

		return "";
	}

/*
=================================================
	GetVulkanSurface
=================================================
*/
	UniquePtr<IVulkanSurface>  WindowSDL2::GetVulkanSurface () const
	{
	#ifdef FG_ENABLE_VULKAN
		return UniquePtr<IVulkanSurface>{new VulkanSurface( _window )};
	#else
		return {};
	#endif
	}
	
/*
=================================================
	GetPlatformHandle
=================================================
*/
	void*  WindowSDL2::GetPlatformHandle () const
	{
		if ( not _window )
			return null;

		SDL_SysWMinfo	info = {};
			
		SDL_VERSION( OUT &info.version );
		CHECK_ERR( SDL_GetWindowWMInfo( _window, OUT &info ) == SDL_TRUE );
		
		switch ( info.subsystem )
		{
			#ifdef PLATFORM_ANDROID
			case SDL_SYSWM_ANDROID :
				return info.info.android.window;
			#endif
				
			#ifdef PLATFORM_WINDOWS
			case SDL_SYSWM_WINDOWS :
				return info.info.win.window;
			#endif
		}
		return null;
	}
//-----------------------------------------------------------------------------


# ifdef FG_ENABLE_VULKAN
/*
=================================================
	VulkanSurface
=================================================
*/
	WindowSDL2::VulkanSurface::VulkanSurface (SDL_Window *wnd) :
		_window{wnd}, _extensions{FGC::VulkanSurface::GetRequiredExtensions()}
	{}
	
/*
=================================================
	Create
=================================================
*/
	IVulkanSurface::SurfaceVk_t  WindowSDL2::VulkanSurface::Create (InstanceVk_t instance) const
	{
		SDL_SysWMinfo	info = {};
			
		SDL_VERSION( OUT &info.version );
		CHECK_ERR( SDL_GetWindowWMInfo( _window, OUT &info ) == SDL_TRUE );
		
		switch ( info.subsystem )
		{
			case SDL_SYSWM_X11 :
				return Zero;	// TODO

			case SDL_SYSWM_WAYLAND :
				return Zero;	// TODO

			case SDL_SYSWM_MIR :
				return Zero;	// TODO

#			if defined(VK_USE_PLATFORM_ANDROID_KHR)
			case SDL_SYSWM_ANDROID :
				return BitCast<SurfaceVk_t>( FGC::VulkanSurface::CreateAndroidSurface( instance, info.info.android.window ));
#			endif
				
#			if defined(PLATFORM_WINDOWS) or defined(VK_USE_PLATFORM_WIN32_KHR)
			case SDL_SYSWM_WINDOWS :
				return BitCast<SurfaceVk_t>( FGC::VulkanSurface::CreateWin32Surface( instance, info.info.win.hinstance, info.info.win.window ));
#			endif
		}

		RETURN_ERR( "current subsystem type is not supported!" );
	}

# endif	// FG_ENABLE_VULKAN

}	// FGC

#endif	// FG_ENABLE_SDL2
