//$ HEADER_NEW_FILE $ 
// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Tts_TtsService_h
#define incl_Tts_TtsService_h

#include "Framework.h"

#include "TtsServiceInterface.h"
#include <QProcess>
#include <QMap>
#include <QPair>
#include <queue>
#include <deque>
#include <time.h>

namespace Tts
{
	using std::queue;
	using std::deque;
	typedef QPair<QString, clock_t> ttsPair;
	
	class TtsService : public TtsServiceInterface
	{
		Q_OBJECT

    public:

		// Constructor
		TtsService(Foundation::Framework* framework_);

        //! Destructor
        virtual ~TtsService();
        
	public slots:

		virtual qulonglong Text2Speech(QString message, QString voice, int type = 1); //1 chat, 2 NPC

		virtual void Text2WAV(QString message, QString pathAndFileName, QString voice);
		virtual void Text2PHO(QString message,QString pathAndFileName, QString voice);


		virtual void File2Speech(QString pathAndFileName, QString voice);
		virtual void File2WAV(QString pathAndFileNameIn, QString pathAndFileNameOut, QString voice);
		virtual void File2PHO(QString pathAndFileNameIn, QString pathAndFileNameOut, QString voice);

		virtual QStringList GetAvailableVoices() const;
		virtual void TriggerSettingsUpdated();

		void TtsAvailable();

		void HandleProcessFinished();

	private:
		
		void InitializeVoices();		
		QString RemoveUnwantedCharacters(QString text);

		//! Framework we belong to
		Foundation::Framework* framework_;
		QMap<QString, QString> voices_;

		//Voice voice_;

		//queue	of voices
		bool tts_available_;
		queue<ttsPair> queue_tts_;
		deque<ttsPair> deque_tts_;
	};
}
#endif