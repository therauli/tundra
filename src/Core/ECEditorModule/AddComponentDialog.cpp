// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "AddComponentDialog.h"
#include "Framework.h"
#include "Scene.h"
#include "SceneAPI.h"
#include "Entity.h"

#include <QLayout>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

#include "MemoryLeakCheck.h"

AddComponentDialog::AddComponentDialog(Framework *fw, const QList<entity_id_t> &ids, QWidget *parent, Qt::WindowFlags f):
    QDialog(parent, f),
    framework_(fw),
    entities_(ids),
    name_line_edit_(0),
    type_combo_box_(0),
    ok_button_(0),
    cancel_button_(0)
{
    setAttribute(Qt::WA_DeleteOnClose);
    if (graphicsProxyWidget())
        graphicsProxyWidget()->setWindowTitle(tr("Add new component"));

    setWindowTitle(tr("Add new component"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5,5,5,5);
    layout->setSpacing(6);

    if(entities_.size() > 1)
    {
        QLabel *component_count_label = new QLabel(QString::number(entities_.size()) + tr(" entities selected."), this);
        layout->addWidget(component_count_label);
    }

    QLabel *component_type_label = new QLabel(tr("Component:"), this);
    QLabel *component_name_label = new QLabel(tr("Name:"), this);
    errorLabel = new QLabel(this);

    name_line_edit_ = new QLineEdit(this);
    type_combo_box_ = new QComboBox(this);
    sync_check_box_ = new QCheckBox(tr("Replicated"), this);
    sync_check_box_->setChecked(true);
    temp_check_box_ = new QCheckBox(tr("Temporary"), this);

    layout->addWidget(component_type_label);
    layout->addWidget(type_combo_box_);
    layout->addWidget(component_name_label);
    layout->addWidget(name_line_edit_);
    layout->addWidget(errorLabel);
    layout->addWidget(sync_check_box_);
    layout->addWidget(temp_check_box_);

    QSpacerItem *spacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->insertSpacerItem(-1, spacer);

    QHBoxLayout *buttons_layout = new QHBoxLayout();
    layout->insertLayout(-1, buttons_layout);
    QSpacerItem *button_spacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttons_layout->addSpacerItem(button_spacer);

    ok_button_ = new QPushButton(tr("Ok"), this);
    cancel_button_ = new QPushButton(tr("Cancel"), this);

    buttons_layout->addWidget(ok_button_);
    buttons_layout->addWidget(cancel_button_);

    connect(name_line_edit_, SIGNAL(textChanged(const QString&)), this, SLOT(CheckComponentName()));
    connect(type_combo_box_, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(CheckComponentName()));
    connect(ok_button_, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancel_button_, SIGNAL(clicked()), this, SLOT(reject()));
}

AddComponentDialog::~AddComponentDialog()
{
}

void AddComponentDialog::SetComponentList(const QStringList &component_types)
{
    foreach(const QString &type, component_types)
        if (type.startsWith("EC_"))
            type_combo_box_->addItem(type.mid(3));
        else
            type_combo_box_->addItem(type);
}

void AddComponentDialog::SetComponentName(const QString &name)
{
    name_line_edit_->setText(name);
}

QString AddComponentDialog::GetTypeName() const
{
    if (!type_combo_box_->currentText().startsWith("EC_"))
        return "EC_" + type_combo_box_->currentText();
    else
        return type_combo_box_->currentText();
}

QString AddComponentDialog::GetName() const
{
    return name_line_edit_->text();
}

bool AddComponentDialog::GetSynchronization() const
{
    return sync_check_box_->isChecked();
}

bool AddComponentDialog::GetTemporary() const
{
    return temp_check_box_->isChecked();
}

QList<entity_id_t> AddComponentDialog::GetEntityIds() const
{
    return entities_;
}

void AddComponentDialog::CheckComponentName()
{
    bool name_duplicates = false;
    Scene *scene = framework_->Scene()->MainCameraScene();
    if(!scene)
        return;

    for(uint i = 0; i < (uint)entities_.size(); i++)
    {
        EntityPtr entity = scene->GetEntity(entities_[i]);
        if (!entity)
            continue;
        QString typeName = type_combo_box_->currentText();
        if (!typeName.startsWith("EC_"))
            typeName.prepend("EC_");
        if (entity->GetComponent(typeName, name_line_edit_->text().trimmed()))
        {
            name_duplicates = true;
            break;
        }
    }

    ok_button_->setDisabled(name_duplicates);
    errorLabel->setText(name_duplicates ? tr("Cannot add components with duplicate names.") : "");
}

void AddComponentDialog::hideEvent(QHideEvent *event)
{
    deleteLater();
}
