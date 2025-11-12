project(alert_app C CXX)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE 99)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)


# Download SDL3
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

set(SDL_SHARED OFF)
set(SDL_STATIC ON)

FetchContent_Declare(
	SDL
	GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
	GIT_TAG preview-3.3.2
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
message(STATUS "Using SDL via FetchContent")
FetchContent_MakeAvailable(SDL)
set_property(DIRECTORY "${sdl_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

# Download SDL_ttf
set(SDLTTF_VENDORED ON)
FetchContent_Declare(
	SDL_ttf
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
	GIT_TAG main
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
message(STATUS "Using SDL_ttf via FetchContent")
FetchContent_MakeAvailable(SDL_ttf)
set_property(DIRECTORY "${sdl_ttf_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

# Download SDL_image
# Just disabling a lot of image formats that are unnecessary to save space
set(SDLIMAGE_VENDORED ON)
set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_GIF ON)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_SVG OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_XV OFF)
set(SDLIMAGE_WEBP OFF)

set(SDLIMAGE_AVIF_SHARED OFF)
set(SDLIMAGE_WEBP_SHARED OFF)
set(SDLIMAGE_TIF_SHARED OFF)
set(SDLIMAGE_JPG_SHARED OFF)
set(SDLIMAGE_JXL_SHARED OFF)
set(SDLIMAGE_PNG_SHARED OFF)

FetchContent_Declare(
	SDL_image
	GIT_REPOSITORY "https://github.com/libsdl-org/SDL_image.git"
	GIT_TAG release-3.2.4
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
message(STATUS "Using SDL_image via FetchContent")


FetchContent_MakeAvailable(SDL_image)
set_property(DIRECTORY "${SDL_image_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)


# SDL_mixer (used for playing audio)
# Disabling what we don't need.
set(SDLMIXER_MIDI_NATIVE OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_WAVPACK OFF)     
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MOD_XMP OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_MP3_MPG123 OFF)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_FLAC_LIBFLAC OFF)
set(SDLMIXER_VENDORED ON)   # tell SDL_mixer to build its own dependencies
FetchContent_Declare(
	SDL3_mixer
	GIT_REPOSITORY "https://github.com/libsdl-org/SDL_mixer.git"
	GIT_TAG main
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
message(STATUS "Using SDL_Mixer via FetchContent")

FetchContent_MakeAvailable(SDL3_mixer)
set_property(DIRECTORY "${SDL_mixer_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)

set(BUILD_SHARED_LIBS TRUE)

set_source_files_properties("${CMAKE_SOURCE_DIR}/include/clay.h" PROPERTIES LANGUAGE C)


add_executable (alert_app "alert_app.cpp" "clayman.cpp")

target_compile_options(alert_app PUBLIC)

target_include_directories(alert_app PRIVATE .)
target_include_directories(alert_app PRIVATE ${CMAKE_SOURCE_DIR}/include)


target_link_libraries(alert_app PRIVATE
	SDL3::SDL3
	SDL3_ttf::SDL3_ttf
	SDL3_image::SDL3_image
	SDL3_mixer::SDL3_mixer
)
