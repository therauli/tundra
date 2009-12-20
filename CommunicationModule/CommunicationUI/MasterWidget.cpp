// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "MasterWidget.h"

#include "LoginHelper.h"
#include "ConfigHelper.h"
#include "SessionManager.h"
#include "EventHandler.h"

#include "ui_LoginWidget.h"
#include "ui_LoadingWidget.h"
#include "ui_SessionManagerWidget.h"

#include <QFile>

namespace CommunicationUI
{
    MasterWidget::MasterWidget(Foundation::Framework *framework)
        : QWidget(),
          ui_state_(UiDefines::UiStates::Disconnected),
          login_helper_(new UiHelpers::LoginHelper),
          config_helper_(new UiHelpers::ConfigHelper(framework)),
          session_manager_(new UiManagers::SessionManager(this)),
          login_ui_(new Ui::LoginWidget),
          loading_ui_(new Ui::LoadingWidget),
          session_manager_ui_(new Ui::SessionManagerWidget()),
          event_handler_(new EventHandler(framework)),
          to_be_deleted_()
    {
		setWindowIcon(QIcon(":/images/iconUsers.png"));
        ChangeContext();
        InitializeSelf();
    }

    MasterWidget::~MasterWidget()
    {
        SAFE_DELETE(login_helper_);
        SAFE_DELETE(config_helper_);
        SAFE_DELETE(session_manager_);
        SAFE_DELETE(event_handler_);
        SAFE_DELETE(login_ui_);
        SAFE_DELETE(loading_ui_);
        SAFE_DELETE(session_manager_ui_);
    }

    void MasterWidget::ChangeContext(UiDefines::UiStates::ConnectionState new_state)
    {
        ui_state_ = (new_state == UiDefines::UiStates::NoStateChange ? ui_state_ : new_state);
        current_size_ = CleanSelf();

        switch (ui_state_)
        {
            case UiDefines::UiStates::Disconnected:
            {
                login_ui_->setupUi(this);
                to_be_deleted_ << "login_ui_";

                config_helper_->SetPreviousData(login_ui_, login_helper_->GetPreviousCredentials());
                login_ui_->passwordLineEdit->setFocus(Qt::MouseFocusReason);
                if (!login_helper_->GetErrorMessage().isEmpty())
                    login_ui_->statusLabel->setText(login_helper_->GetErrorMessage());
                
                connect(login_ui_->connectPushButton, SIGNAL( clicked() ), login_helper_, SLOT( TryLogin() ));
                connect(login_helper_, SIGNAL( StateChange(UiDefines::UiStates::ConnectionState) ), this, SLOT( ChangeContext(UiDefines::UiStates::ConnectionState) ));
                break;
            }
            case UiDefines::UiStates::Connecting:
            {
                loading_ui_->setupUi(this);
                to_be_deleted_ << "loading_ui_";

                connect(loading_ui_->cancelPushButton, SIGNAL( clicked() ), login_helper_, SLOT( LoginCanceled() ));
                break;
            }
            case UiDefines::UiStates::Connected:
            {
                config_helper_->SaveLoginData(login_helper_->GetPreviousCredentials());
                session_manager_ui_->setupUi(this);
                to_be_deleted_ << "session_manager_ui_";

                connect(session_manager_, SIGNAL( StateChange(UiDefines::UiStates::ConnectionState) ), this, SLOT( ChangeContext(UiDefines::UiStates::ConnectionState) ));
                session_manager_->Start(login_helper_->GetPreviousCredentials()["username"], login_helper_->GetConnectionInterface(), event_handler_);
                break;
            }
            case UiDefines::UiStates::Exit:
            {
                close();
                return;
            }
        }
        resize(current_size_);
    }

    void MasterWidget::InitializeSelf()
    {
        // Read config and setup ui pointers for helpers
        config_helper_->ReadLoginData(login_ui_);
        login_helper_->SetupUi(login_ui_);
        session_manager_->SetupUi(session_manager_ui_);

        // Read main style sheet from file
        QFile style_sheet_file("../CommunicationModule/CommunicationUI/ui/StyleSheet.css");
        if (style_sheet_file.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString(style_sheet_file.readAll()));

        // Set initial size
        resize(350, 120);
    }

    QSize MasterWidget::CleanSelf()
    {
        setObjectName("");

        foreach (QString ui_class, to_be_deleted_)
        {
            if (ui_class == "login_ui_")
			{
				SAFE_DELETE(login_ui_->verticalLayout_2);
				SAFE_DELETE(login_ui_->connectPushButton);
				SAFE_DELETE(login_ui_->MainFrame);
			}
            if (ui_class == "loading_ui_")
            {
                SAFE_DELETE(loading_ui_->verticalLayout_2);
				SAFE_DELETE(loading_ui_->cancelPushButton);
				SAFE_DELETE(loading_ui_->label);
                SAFE_DELETE(loading_ui_->MainFrame);
            }
            if (ui_class == "session_manager_ui_")
                SAFE_DELETE(session_manager_ui_->mainVerticalLayout);
        }
        to_be_deleted_.clear();

        return size();
    }
}