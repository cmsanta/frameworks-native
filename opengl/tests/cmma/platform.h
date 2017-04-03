#ifndef PLATFORM_H
#define PLATFORM_H


namespace MaliSDK
{

	class Platform 
	{
	public:
	 /**
	 * \brief Print a log message to the terminal.
	 * \param[in] format The format the log message should take. Equivilent to printf.
	 * \param[in] ... Variable length input to specify variables to print. They will be formatted as specified in format.
	 */
	   static void log(const char* format, ...);
	/**
	 * \brief Checks if OpenGL ES has reported any errors.
	 * \param[in] operation The OpenGL ES function that has been called.
	 */
	static void checkGlesError(const char* operation);

	/**
	 * \brief Converts OpenGL ES error codes into the readable strings.
	 * \param[in] glErrorCode The OpenGL ES error code to convert.
	 * \return The string form of the error code.
	 */
	static const char* glErrorToString(int glErrorCode);

};

#define LOGI Platform::log 
#define LOGE fprintf (stderr, "Error: "); Platform::log
#ifdef DEBUG
#define LOGD fprintf (stderr, "Debug: "); Platform::log
#else
#define LOGD
#endif

#define GL_CHECK(x) \
        x; \
        Platform::checkGlesError(#x);

}
#endif /* PLATFORM_H */
