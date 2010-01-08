#include "StableHeaders.h"

#include "EnvironmentModule.h"
#include "TerrainDecoder.h"
#include "Terrain.h"
#include "EnvironmentEditor.h"
#include "TerrainLabel.h"
#include "Water.h"
#include "Environment.h"

#include "TextureInterface.h"
#include "TextureServiceInterface.h"

#include <UiModule.h>
#include <UiProxyWidget.h>
#include <UiWidgetProperties.h>

#include <QUiLoader>
#include <QPushButton>
#include <QImage>
#include <QLabel>
#include <QColor>
#include <QMouseEvent>
#include <QRadioButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QColorDialog>

namespace Environment
{
    EnvironmentEditor::EnvironmentEditor(EnvironmentModule* environment_module):
    environment_module_(environment_module),
    editor_widget_(0),
    action_(Flatten),
    brush_size_(Small),
    ambient_(false)
    //mouse_press_flag_(no_button)
    {
        // Those two arrays size should always be the same as how many terrain textures we are using.
       
        terrain_texture_id_list_.resize(cNumberOfTerrainTextures);
        terrain_texture_requests_.resize(cNumberOfTerrainTextures);
        terrain_ = environment_module_->GetTerrainHandler();
        water_ = environment_module_->GetWaterHandler();
        environment_ = environment_module->GetEnvironmentHandler();

        InitEditorWindow();

       
    }

    EnvironmentEditor::~EnvironmentEditor()
    {
        EnvironmentEditorProxyWidget_ = 0;
        delete color_picker_;
        color_picker_ = 0;
    }

    void EnvironmentEditor::CreateHeightmapImage()
    {
        if(terrain_.get() && editor_widget_)
        {
            Scene::EntityPtr entity = terrain_->GetTerrainEntity().lock();
            EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();

            if(terrain_component->AllPatchesLoaded())
            {
                // Find image label in widget so we can get the basic information about the image that we are about to update.
                QLabel *label = editor_widget_->findChild<QLabel *>("map_label");
                const QPixmap *pixmap = label->pixmap();
                QImage image = pixmap->toImage();

                // Make sure that image is in right size.
                if(image.height() != cHeightmapImageHeight || image.width() != cHeightmapImageWidth)
                    return;

                // Generate image based on heightmap values. The Highest value on heightmap will show image as white color and the lowest as black color.
                MinMaxValue values = GetMinMaxHeightmapValue(*terrain_component);
                for(int height = 0; height < image.height(); height++)
                {
                    for(int width = 0; width < image.width(); width++)
                    {
                        float value = (terrain_component->GetPoint(width, height) - values.first);
                        value /= (values.second - values.first);
                        value *= 255;
                        QRgb color_value = qRgb(value, value, value);
                        image.setPixel(width, height, color_value);
                    }
                }

                // Set new image into the label.
                label->setPixmap(QPixmap::fromImage(image));
            }
        }
    }

    MinMaxValue EnvironmentEditor::GetMinMaxHeightmapValue(EC_Terrain &terrain) const
    {
        float min, max;
        min = 65535.0f;
        max = 0.0f;

        MinMaxValue values;

        if(terrain.AllPatchesLoaded())
        {
            for(int i = 0; i < cHeightmapImageWidth; i++)
            {
                for(int j = 0; j < cHeightmapImageHeight; j++)
                {
                    float height_value = terrain.GetPoint(i, j);
                    if(height_value < min)
                    {
                        min = height_value;
                        values.first = height_value;
                    }
                    else if(height_value > max)
                    {
                        max = height_value;
                        values.second = height_value;
                    }
                }
            }
        }
        return values;
    }

    void EnvironmentEditor::InitEditorWindow()
    {
        QUiLoader loader;
        QFile file("./data/ui/environment_editor.ui");
        if(!file.exists())
        {
            EnvironmentModule::LogError("Cannot find terrain editor ui file");
            return;
        }
        editor_widget_ = loader.load(&file);
        file.close();

        boost::shared_ptr<UiServices::UiModule> ui_module = environment_module_->GetFramework()->GetModuleManager()->GetModule<UiServices::UiModule>(Foundation::Module::MT_UiServices).lock();
        if (!ui_module.get())
            return;

        EnvironmentEditorProxyWidget_ = 
            ui_module->GetSceneManager()->AddWidgetToCurrentScene(editor_widget_, UiServices::UiWidgetProperties(QPointF(60,60), editor_widget_->size(), Qt::Dialog, "Environment Editor"));

        QWidget *map_widget = editor_widget_->findChild<QWidget *>("map_widget");
        if(map_widget)
        {
            TerrainLabel *label = new TerrainLabel(map_widget, 0);
            label->setObjectName("map_label");
            QObject::connect(label, SIGNAL(SendMouseEvent(QMouseEvent*)), this, SLOT(HandleMouseEvent(QMouseEvent*)));
            //label->resize(map_widget->size());

            // Create a QImage object and set it in label.
            QImage heightmap(cHeightmapImageWidth, cHeightmapImageHeight, QImage::Format_RGB32);
            heightmap.fill(0);
            label->setPixmap(QPixmap::fromImage(heightmap));
        }

        // Button Signals
        QPushButton *update_button = editor_widget_->findChild<QPushButton *>("button_update");
        QPushButton *apply_button_one = editor_widget_->findChild<QPushButton *>("apply_button_1");
        QPushButton *apply_button_two = editor_widget_->findChild<QPushButton *>("apply_button_2");
        QPushButton *apply_button_three = editor_widget_->findChild<QPushButton *>("apply_button_3");
        QPushButton *apply_button_four = editor_widget_->findChild<QPushButton *>("apply_button_4");

        QObject::connect(update_button, SIGNAL(clicked()), this, SLOT(UpdateTerrain()));
        QObject::connect(apply_button_one, SIGNAL(clicked()), this, SLOT(ApplyButtonPressed()));
        QObject::connect(apply_button_two, SIGNAL(clicked()), this, SLOT(ApplyButtonPressed()));
        QObject::connect(apply_button_three, SIGNAL(clicked()), this, SLOT(ApplyButtonPressed()));
        QObject::connect(apply_button_four, SIGNAL(clicked()), this, SLOT(ApplyButtonPressed()));

        // RadioButton Signals
        QRadioButton *rad_button_flatten = editor_widget_->findChild<QRadioButton *>("rad_button_flatten");
        QRadioButton *rad_button_raise = editor_widget_->findChild<QRadioButton *>("rad_button_raise");
        QRadioButton *rad_button_lower = editor_widget_->findChild<QRadioButton *>("rad_button_lower");
        QRadioButton *rad_button_smooth = editor_widget_->findChild<QRadioButton *>("rad_button_smooth");
        QRadioButton *rad_button_roughen = editor_widget_->findChild<QRadioButton *>("rad_button_roughen");
        QRadioButton *rad_button_revert = editor_widget_->findChild<QRadioButton *>("rad_button_revert");

        QObject::connect(rad_button_flatten, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));
        QObject::connect(rad_button_raise, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));
        QObject::connect(rad_button_lower, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));
        QObject::connect(rad_button_smooth, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));
        QObject::connect(rad_button_roughen, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));
        QObject::connect(rad_button_revert, SIGNAL(clicked()), this, SLOT(PaintActionChanged()));

        QRadioButton *rad_button_small = editor_widget_->findChild<QRadioButton *>("rad_button_small");
        QRadioButton *rad_button_medium = editor_widget_->findChild<QRadioButton *>("rad_button_medium");
        QRadioButton *rad_button_large = editor_widget_->findChild<QRadioButton *>("rad_button_large");

        QObject::connect(rad_button_small, SIGNAL(clicked()), this, SLOT(BrushSizeChanged()));
        QObject::connect(rad_button_medium, SIGNAL(clicked()), this, SLOT(BrushSizeChanged()));
        QObject::connect(rad_button_large, SIGNAL(clicked()), this, SLOT(BrushSizeChanged()));

        // Tab window signals
        QTabWidget *tab_widget = editor_widget_->findChild<QTabWidget *>("tabWidget");
        QObject::connect(tab_widget, SIGNAL(currentChanged(int)), this, SLOT(TabWidgetChanged(int)));

        // Line Edit signals
        QLineEdit *line_edit_one = editor_widget_->findChild<QLineEdit *>("texture_line_edit_1");
        QLineEdit *line_edit_two = editor_widget_->findChild<QLineEdit *>("texture_line_edit_2");
        QLineEdit *line_edit_three = editor_widget_->findChild<QLineEdit *>("texture_line_edit_3");
        QLineEdit *line_edit_four = editor_widget_->findChild<QLineEdit *>("texture_line_edit_4");

        QObject::connect(line_edit_one, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));
        QObject::connect(line_edit_two, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));
        QObject::connect(line_edit_three, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));
        QObject::connect(line_edit_four, SIGNAL(returnPressed()), this, SLOT(LineEditReturnPressed()));

        // Double spin box signals
        QDoubleSpinBox *double_spin_one = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_doubleSpinBox_1");
        QDoubleSpinBox *double_spin_two = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_doubleSpinBox_2");
        QDoubleSpinBox *double_spin_three = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_doubleSpinBox_3");
        QDoubleSpinBox *double_spin_four = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_doubleSpinBox_4");

        QObject::connect(double_spin_one, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
        QObject::connect(double_spin_two, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
        QObject::connect(double_spin_three, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
        QObject::connect(double_spin_four, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));

        double_spin_one = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_range_doubleSpinBox_1");
        double_spin_two = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_range_doubleSpinBox_2");
        double_spin_three = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_range_doubleSpinBox_3");
        double_spin_four = editor_widget_->findChild<QDoubleSpinBox *>("Texture_height_range_doubleSpinBox_4");

        QObject::connect(double_spin_one, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
        QObject::connect(double_spin_two, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
        QObject::connect(double_spin_three, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
        QObject::connect(double_spin_four, SIGNAL(valueChanged(double)), this, SLOT(HeightValueChanged(double)));
    
        // Water editing button connections.

        QPushButton* water_apply_button = editor_widget_->findChild<QPushButton *>("water_apply_button");
        QDoubleSpinBox* water_height_box = editor_widget_->findChild<QDoubleSpinBox* >("water_height_doublespinbox");
        QCheckBox* water_toggle_box = editor_widget_->findChild<QCheckBox* >("water_toggle_box");

        if ( water_apply_button != 0 && water_height_box != 0 && water_toggle_box != 0 )
        {
            water_height_box->setMinimum(0.0);
            QObject::connect(water_apply_button, SIGNAL(clicked()), this, SLOT(UpdateWaterHeight()));
            
            if ( water_.get() != 0)
            {
                // Initialize
                double height = water_->GetWaterHeight();
                water_height_box->setValue(height);
                water_toggle_box->setChecked(true);
                // If water is created after, this connection must be made!
                QObject::connect(water_.get(), SIGNAL(HeightChanged(double)), water_height_box, SLOT(setValue(double)));
                // Idea here is that if for some reason server removes water it state is updated to editor correctly.
                QObject::connect(water_.get(), SIGNAL(WaterRemoved()), this, SLOT(ToggleWaterCheckButton()));
                QObject::connect(water_.get(), SIGNAL(WaterCreated()), this, SLOT(ToggleWaterCheckButton()));
              
            }
            else
            {
                // Water is not created adjust a initial values
                water_toggle_box->setChecked(false);
                water_height_box->setValue(0.0);
            }
            QObject::connect(water_toggle_box, SIGNAL(stateChanged(int)), this, SLOT(UpdateWaterGeometry(int)));
            
        }

        
        // Defines that is override fog color value used. 
        QCheckBox* fog_override_box = editor_widget_->findChild<QCheckBox* >("fog_override_checkBox");
        
        // Ground fog
        QDoubleSpinBox* fog_ground_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_red_dSpinBox");
        QDoubleSpinBox* fog_ground_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_blue_dSpinBox");
        QDoubleSpinBox* fog_ground_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_green_dSpinBox");
        QPushButton* fog_ground_color_button = editor_widget_->findChild<QPushButton *>("fog_ground_color_apply_button");
        QDoubleSpinBox* fog_ground_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_start_distance_dSpinBox");
        QDoubleSpinBox* fog_ground_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_end_distance_dSpinBox");
        QPushButton* fog_ground_distance_button = editor_widget_->findChild<QPushButton *>("fog_ground_distance_apply_button");

        // Water fog
        QDoubleSpinBox* fog_water_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_red_dSpinBox");
        QDoubleSpinBox* fog_water_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_blue_dSpinBox");
        QDoubleSpinBox* fog_water_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_green_dSpinBox");
        QPushButton* fog_water_color_button = editor_widget_->findChild<QPushButton *>("fog_water_color_apply_button");
        QDoubleSpinBox* fog_water_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_start_distance_dSpinBox");
        QDoubleSpinBox* fog_water_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_end_distance_dSpinBox");
        QPushButton* fog_water_distance_button = editor_widget_->findChild<QPushButton *>("fog_water_distance_apply_button");

        
        if ( environment_ != 0)
        {
            if ( fog_override_box != 0 )
            {
                if ( environment_->GetFogColorOverride() )
                    fog_override_box->setChecked(true);
                else
                    fog_override_box->setChecked(false);

                QObject::connect(fog_override_box, SIGNAL(clicked()), this, SLOT(ToggleFogOverride()));
            }     
             
            if ( fog_ground_red != 0 
                && fog_ground_blue != 0
                && fog_ground_green != 0
                && fog_ground_color_button != 0
                && fog_ground_start_distance != 0
                && fog_ground_end_distance != 0
                && fog_ground_distance_button != 0)
            {
                    // Fog ground color. 
                    QVector<float> color = environment_->GetFogGroundColor();
                    
                    fog_ground_red->setMinimum(0.0);
                    fog_ground_red->setValue(color[0]);   

                    fog_ground_blue->setMinimum(0.0);
                    fog_ground_blue->setValue(color[1]);

                    fog_ground_green->setMinimum(0.0);
                    fog_ground_green->setValue(color[2]);

                    QObject::connect(environment_.get(), SIGNAL(GroundFogAdjusted(float, float, const QVector<float>&)), this, SLOT(UpdateGroundFog(float, float, const QVector<float>&)));
                    QObject::connect(fog_ground_color_button, SIGNAL(clicked()), this, SLOT(SetGroundFog()));
                
                    fog_ground_start_distance->setMinimum(0.0);
                    fog_ground_end_distance->setMinimum(0.0);

                    QObject::connect(fog_ground_distance_button, SIGNAL(clicked()), this, SLOT(SetGroundFogDistance()));
                    fog_ground_start_distance->setMaximum(1000.0);
                    fog_ground_start_distance->setValue(environment_->GetGroundFogStartDistance());
                    fog_ground_end_distance->setMaximum(1000.0);
                    fog_ground_end_distance->setValue(environment_->GetGroundFogEndDistance());
             }

           if ( fog_water_red != 0 
                && fog_water_blue != 0
                && fog_water_green != 0
                && fog_water_color_button != 0
                && fog_water_start_distance != 0
                && fog_water_end_distance != 0
                && fog_water_distance_button != 0)
            {
                    // Fog water color. 
                    QVector<float> color = environment_->GetFogWaterColor();
                    
                    fog_water_red->setMinimum(0.0);
                    fog_water_red->setValue(color[0]);   

                    fog_water_blue->setMinimum(0.0);
                    fog_water_blue->setValue(color[1]);

                    fog_water_green->setMinimum(0.0);
                    fog_water_green->setValue(color[2]);

                    QObject::connect(environment_.get(), SIGNAL(WaterFogAdjusted(float, float, const QVector<float>&)), this, SLOT(UpdateWaterFog(float, float, const QVector<float>&)));
                    QObject::connect(fog_water_color_button, SIGNAL(clicked()), this, SLOT(SetWaterFog()));
        
                    fog_water_start_distance->setMinimum(0.0);
                    fog_water_end_distance->setMinimum(0.0);

                    QObject::connect(fog_water_distance_button, SIGNAL(clicked()), this, SLOT(SetWaterFogDistance()));
                    fog_water_start_distance->setMaximum(1000.0);
                    fog_water_start_distance->setValue(environment_->GetWaterFogStartDistance());
                    fog_water_end_distance->setMaximum(1000.0);
                    fog_water_end_distance->setValue(environment_->GetWaterFogEndDistance());

            }


            // Sun direction
            QDoubleSpinBox* sun_direction_x = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_x");
            sun_direction_x->setMinimum(-100.0);
            QDoubleSpinBox* sun_direction_y  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_y");
            sun_direction_y->setMinimum(-100.0);
            QDoubleSpinBox* sun_direction_z  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_z");
            sun_direction_z->setMinimum(-100.0);
            
          
            color_picker_ = new QColorDialog;
            if ( sun_direction_x != 0
                 && sun_direction_y != 0
                 && sun_direction_z != 0
                
                 )
            {
                // Initialize sun direction value
                QVector<float> sun_direction = environment_->GetSunDirection();
                sun_direction_x->setValue(sun_direction[0]);
                sun_direction_y->setValue(sun_direction[1]);
                sun_direction_z->setValue(sun_direction[2]);
                
                QObject::connect(sun_direction_x, SIGNAL(valueChanged(double)), this, SLOT(UpdateSunDirection(double)));
                QObject::connect(sun_direction_y, SIGNAL(valueChanged(double)), this, SLOT(UpdateSunDirection(double)));
                QObject::connect(sun_direction_z, SIGNAL(valueChanged(double)), this, SLOT(UpdateSunDirection(double)));

                QVector<float> sun_color = environment_->GetSunColor();
              
                
                QLabel* label = editor_widget_->findChild<QLabel* >("sun_color_img");
                if ( label != 0)
                {
                   
                    QPixmap img(label->width(), 23);
                    QColor color;
                    color.setRedF(sun_color[0]);
                    color.setGreenF(sun_color[1]);
                    color.setBlueF(sun_color[2]);
                    //color.setAlpha(sun_color[3]);
                    //color.setAlpha(0);
                    img.fill(color);
                    label->setPixmap(img);
                }
                
             
              
                QPushButton* sun_color_change = editor_widget_->findChild<QPushButton* >("sun_color_change_button");
                if ( sun_color_change != 0)
                    QObject::connect(sun_color_change, SIGNAL(clicked()), this, SLOT(ShowColorPicker()));

                QPushButton* ambient_color_change = editor_widget_->findChild<QPushButton* >("ambient_color_change_button");
                if ( ambient_color_change != 0)
                    QObject::connect(ambient_color_change, SIGNAL(clicked()), this, SLOT(ShowColorPicker()));
                
                QVector<float> ambient_light = environment_->GetAmbientLight();
                label = editor_widget_->findChild<QLabel* >("ambient_color_img");
                if ( label != 0)
                {
                   
                    QPixmap img(label->width(), 23);
                    QColor color;
                    color.setRedF(ambient_light[0]);
                    color.setGreenF(ambient_light[1]);
                    color.setBlueF(ambient_light[2]);
                    //color.setAlpha(sun_color[3]);
                    //color.setAlpha(0);
                    img.fill(color);
                    label->setPixmap(img);
                }

              

            }
            QObject::connect(color_picker_, SIGNAL(currentColorChanged(const QColor& )), this, SLOT(UpdateColor(const QColor&)));

                
        }


        


    }

    void EnvironmentEditor::UpdateColor(const QColor& color)
    {
        QWidget* widget = GetCurrentPage();
        if ( widget != 0 
             && widget->objectName() == "ambient_light")
        {
            if ( !ambient_ ) 
            {
                QVector<float> current_sun_color = environment_->GetSunColor();
                QVector<float> new_sun_color(4);
                new_sun_color[0] = color.redF(), new_sun_color[1] = color.greenF(), new_sun_color[2] = color.blueF(), new_sun_color[3] = 1;
                if ( new_sun_color[0] != current_sun_color[0] 
                     && new_sun_color[1] != current_sun_color[1] 
                     && new_sun_color[2] != current_sun_color[2])
                {
                    // Change to new color.
                    environment_->SetSunColor(new_sun_color);
                    QLabel* label = editor_widget_->findChild<QLabel* >("sun_color_img");
                    if ( label != 0)
                    {
                        QPixmap img(label->width(), 23);
                        QColor color;
                        color.setRedF(new_sun_color[0]);
                        color.setGreenF(new_sun_color[1]);
                        color.setBlueF(new_sun_color[2]);
                        //color.setAlpha(sun_color[3]);
                        //color.setAlpha(0);
                        img.fill(color);
                        label->setPixmap(img);
                    }
                }

            }
            else
            {
                // ambient light.
                QVector<float> current_ambient_color = environment_->GetAmbientLight();
                QVector<float> new_ambient_color(3);
                new_ambient_color[0] = color.redF(), new_ambient_color[1] = color.greenF(), new_ambient_color[2] = color.blueF();
                if ( new_ambient_color[0] != current_ambient_color[0] 
                     && new_ambient_color[1] != current_ambient_color[1] 
                     && new_ambient_color[2] != current_ambient_color[2])
                {
                    // Change to new color.
                    environment_->SetAmbientLight(new_ambient_color);
                    QLabel* label = editor_widget_->findChild<QLabel* >("ambient_color_img");
                    if ( label != 0)
                    {
                        QPixmap img(label->width(), 23);
                        QColor color;
                        color.setRedF(new_ambient_color[0]);
                        color.setGreenF(new_ambient_color[1]);
                        color.setBlueF(new_ambient_color[2]);
                        //color.setAlpha(sun_color[3]);
                        //color.setAlpha(0);
                        img.fill(color);
                        label->setPixmap(img);
                    }
                }

                     ambient_ = false;
            }


        }

    }


    void EnvironmentEditor::ShowColorPicker()
    {
        QWidget* widget = GetCurrentPage();
        if ( widget != 0 && widget->objectName() == "ambient_light"
                && environment_ != 0 )
        {
            if (this->sender()->objectName() == "sun_color_change_button")
            {
                QVector<float> sun_color = environment_->GetSunColor();
                QColor color;
                color.setRedF(sun_color[0]);
                color.setGreenF(sun_color[1]);
                color.setBlueF(sun_color[2]);
                //color.setAlpha(0);
                color_picker_->setCurrentColor(color);
            }

            if ( this->sender()->objectName() == "ambient_color_change_button")
            {
                QVector<float> ambient_light = environment_->GetAmbientLight();
                QColor color;
                color.setRedF(ambient_light[0]);
                color.setGreenF(ambient_light[1]);
                color.setBlueF(ambient_light[2]);
                color_picker_->setCurrentColor(color);
                ambient_ = true;
            }
         }
            
       color_picker_->show();
    }

    QWidget* EnvironmentEditor::GetPage(const QString& name)
    {
         
        QTabWidget* tab_widget = editor_widget_->findChild<QTabWidget* >("tabWidget");
        if ( tab_widget != 0 )
        {
            for ( int i = 0; i < tab_widget->count(); ++i)
            {
                QWidget* page = tab_widget->widget(i);
                if ( page != 0 && page->objectName() == name)
                    return page;
                
            }
        }

        return 0;
    }

    QWidget* EnvironmentEditor::GetCurrentPage()
    {
        QWidget* page = 0;
        QTabWidget* tab_widget = editor_widget_->findChild<QTabWidget* >("tabWidget");
        if ( tab_widget != 0 )
            page = tab_widget->currentWidget();
            
        return page;

    }

    void EnvironmentEditor::UpdateSunDirection(double value)
    {
        // Sun direction

        QDoubleSpinBox* sun_direction_x = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_x");
        QDoubleSpinBox* sun_direction_y  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_y");
        QDoubleSpinBox* sun_direction_z  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_z");
       
        if ( sun_direction_x != 0 &&
             sun_direction_y != 0 &&
             sun_direction_z != 0 &&
             environment_ != 0)
        {
         QVector<float> sun_direction(3);
         sun_direction[0] = static_cast<float>(sun_direction_x->value());
         sun_direction[1] = static_cast<float>(sun_direction_y->value());
         sun_direction[2] = static_cast<float>(sun_direction_z->value());
         environment_->SetSunDirection(sun_direction);
        }

    }

    void EnvironmentEditor::UpdateWaterHeight()
    {
        // Sanity check: if water is totally disabled do not update it. 
        QCheckBox* water_toggle_box = editor_widget_->findChild<QCheckBox* >("water_toggle_box");

        if (water_toggle_box != 0 && !water_toggle_box->isChecked())
            return;

        QDoubleSpinBox* water_height_box = editor_widget_->findChild<QDoubleSpinBox* >("water_height_doublespinbox");
        if ( water_.get() != 0 && water_height_box != 0)
            water_->SetWaterHeight(static_cast<float>(water_height_box->value())); 
    }

    void EnvironmentEditor::UpdateWaterGeometry(int state)
    {
        QDoubleSpinBox* water_height_box = editor_widget_->findChild<QDoubleSpinBox* >("water_height_doublespinbox");
        
        switch ( state )
        {
        case Qt::Checked:
            {
                if ( water_height_box != 0 && water_.get() != 0 )
                    water_->CreateWaterGeometry(static_cast<float>(water_height_box->value()));
                else if ( water_.get() != 0)
                    water_->CreateWaterGeometry();
                
                break;
            }
        case Qt::Unchecked:
            {
                if ( water_.get() != 0)
                    water_->RemoveWaterGeometry();
                 break;
            }
        default:
            break;

        }
      

    }

    void EnvironmentEditor::ToggleWaterCheckButton()
    {
        QCheckBox* water_toggle_box = editor_widget_->findChild<QCheckBox* >("water_toggle_box");
        

        // Dirty way to check that what is state of water. 
        
        if ( water_.get() != 0 && water_toggle_box != 0)
        {
        
            if ( water_->GetWaterEntity().expired())
                water_toggle_box->setChecked(false); 
            else
              water_toggle_box->setChecked(true);
        }
       
            
    }

    void EnvironmentEditor::UpdateTerrainTextureRanges()
    {
        if(!terrain_.get())
            return;

        for(uint i = 0; i < cNumberOfTerrainTextures; i++)
        {
            Real start_height_value = terrain_->GetTerrainTextureStartHeight(i);
            Real height_range_value = terrain_->GetTerrainTextureHeightRange(i);

            QString start_height_name("Texture_height_doubleSpinBox_" + QString("%1").arg(i + 1));
            QString height_range_name("Texture_height_range_doubleSpinBox_" + QString("%1").arg(i + 1));

            QDoubleSpinBox *start_height_spin = editor_widget_->findChild<QDoubleSpinBox *>(start_height_name);
            if(start_height_spin)
                start_height_spin->setValue(start_height_value);

            QDoubleSpinBox *height_range_spin = editor_widget_->findChild<QDoubleSpinBox *>(height_range_name);
            if(height_range_spin)
                height_range_spin->setValue(height_range_value);
        }
    }

    void EnvironmentEditor::LineEditReturnPressed()
    {
        if(!terrain_.get())
        {
            EnvironmentModule::LogError("Cannot change terrain texture cause pointer to terrain isn't initialized on editor yet.");
            return;
        }

        const QLineEdit *sender = qobject_cast<QLineEdit *>(QObject::sender());
        int line_edit_number = 0;
        if(sender)
        {
            // Make sure that the signal sender was some of those line edit widgets.
            if(sender->objectName()      == "texture_line_edit_1") line_edit_number = 0;
            else if(sender->objectName() == "texture_line_edit_2") line_edit_number = 1;
            else if(sender->objectName() == "texture_line_edit_3") line_edit_number = 2;
            else if(sender->objectName() == "texture_line_edit_4") line_edit_number = 3;

            if(line_edit_number >= 0)
            {
                if(terrain_texture_id_list_[line_edit_number] != sender->text().toStdString())
                {
                    terrain_texture_id_list_[line_edit_number] = sender->text().toStdString();
                    RexTypes::RexAssetID texture_id[cNumberOfTerrainTextures];
                    for(uint i = 0; i < cNumberOfTerrainTextures; i++)
                        texture_id[i] = terrain_texture_id_list_[i];
                    terrain_->SetTerrainTextures(texture_id);

                    terrain_texture_requests_[line_edit_number] = RequestTerrainTexture(line_edit_number);
                }
            }
        }
    }

    void EnvironmentEditor::ApplyButtonPressed()
    {
        if(!terrain_.get())
        {
            EnvironmentModule::LogError("Cannot change terrain texture cause pointer to terrain isn't initialized on editor yet.");
            return;
        }

        const QPushButton *sender = qobject_cast<QPushButton *>(QObject::sender());
        int button_number = 0;
        if(sender)
        {
            // Make sure that the signal sender was some of those apply buttons.
            if(sender->objectName()      == "apply_button_1") button_number = 0;
            else if(sender->objectName() == "apply_button_2") button_number = 1;
            else if(sender->objectName() == "apply_button_3") button_number = 2;
            else if(sender->objectName() == "apply_button_4") button_number = 3;

            if(button_number >= 0)
            {
                QString line_edit_name("texture_line_edit_" + QString("%1").arg(button_number + 1));
                QLineEdit *line_edit = editor_widget_->findChild<QLineEdit*>(line_edit_name);
                if(terrain_texture_id_list_[button_number] != line_edit->text().toStdString())
                {
                    terrain_texture_id_list_[button_number] = line_edit->text().toStdString();
                    RexTypes::RexAssetID texture_id[cNumberOfTerrainTextures];
                    for(uint i = 0; i < cNumberOfTerrainTextures; i++)
                        texture_id[i] = terrain_texture_id_list_[i];
                    terrain_->SetTerrainTextures(texture_id);

                    terrain_texture_requests_[button_number] = RequestTerrainTexture(button_number);

                    environment_module_->SendTextureDetailMessage(texture_id[button_number], button_number);
                }
            }
        }


        QString start_height("Texture_height_doubleSpinBox_" + QString("%1").arg(button_number + 1));
        QString height_range("Texture_height_range_doubleSpinBox_" + QString("%1").arg(button_number + 1));
        QDoubleSpinBox *start_height_spin = editor_widget_->findChild<QDoubleSpinBox*>(start_height);
        QDoubleSpinBox *height_range_spin = editor_widget_->findChild<QDoubleSpinBox*>(height_range);
        if(start_height_spin && height_range_spin)
            environment_module_->SendTextureHeightMessage(start_height_spin->value(), height_range_spin->value(), button_number);
    }

    void EnvironmentEditor::HeightValueChanged(double height)
    {
        //environment_module_->SendTextureHeightMessage();
    }

    void EnvironmentEditor::UpdateTerrain()
    {
        // We only need to update terrain information when window is visible.
        /*
        if(canvas_.get())
        {
            if(canvas_->IsHidden())
            {
                return;
            }
        }
        */

        assert(environment_module_);
        if(!environment_module_)
        {
            EnvironmentModule::LogError("Can't update terrain because environment module is not intialized.");
            return;
        }

        terrain_ = environment_module_->GetTerrainHandler();
        if(!terrain_)
            return;

        CreateHeightmapImage();
    }

    void EnvironmentEditor::HandleMouseEvent(QMouseEvent *ev)
    {
        // Ugly this need to be removed when mouse move events are working corretly in Rex UICanvas.
        // Check if mouse has pressed.
        /*if(ev->type() == QEvent::MouseButtonPress)
        {
            QPoint position = ev->pos();
            Scene::EntityPtr entity = terrain_->GetTerrainEntity().lock();
            EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
            start_height_ = terrain_component->GetPoint(position.x(), position.y());

            switch(ev->button())
            {
                case Qt::LeftButton:
                    mouse_press_flag_ |= left_button;
                    break;
                case Qt::RightButton:
                    mouse_press_flag_ |= right_button;
                    break;
                case Qt::MidButton:
                    mouse_press_flag_ |= middle_button;
                    break;
            }
        }
        else if(ev->type() == QEvent::MouseButtonRelease)
        {
            switch(ev->button())
            {
                case Qt::LeftButton:
                    mouse_press_flag_ -= left_button;
                    break;
                case Qt::RightButton:
                    mouse_press_flag_ -= right_button;
                    break;
                case Qt::MidButton:
                    mouse_press_flag_ -= middle_button;
                    break;
            }
        }

        if(mouse_press_flag_ & button_mask > 0)
        {
                QLabel *label = editor_widget_->findChild<QLabel *>("map_label");
                const QPixmap *pixmap = label->pixmap();
                QImage image = pixmap->toImage();
                image.setPixel(ev->pos(), qRgb(255, 255, 255));
                label->setPixmap(QPixmap::fromImage(image));
                label->show();

                QPoint position = ev->pos();
                Scene::EntityPtr entity = terrain_->GetTerrainEntity().lock();
                EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
                environment_module_->SendModifyLandMessage(position.x(), position.y(), brush_size_, action_, 0.15f, start_height_);
        }*/

        if(ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseMove)
        {
            if(ev->button() == Qt::LeftButton)
            {
                // Draw a white point where we have clicked.
                /*QLabel *label = editor_widget_->findChild<QLabel *>("map_label");
                const QPixmap *pixmap = label->pixmap();
                QImage image = pixmap->toImage();
                image.setPixel(ev->pos(), qRgb(255, 255, 255));
                label->setPixmap(QPixmap::fromImage(image));
                label->show();*/

                // Send modify land message to server.
                QPoint position = ev->pos();
                if(!terrain_.get())
                    return;

                Scene::EntityPtr entity = terrain_->GetTerrainEntity().lock();
                EC_Terrain *terrain_component = entity->GetComponent<EC_Terrain>().get();
                if(!terrain_component)
                    return;

                environment_module_->SendModifyLandMessage(position.x(), position.y(), brush_size_, action_, 0.15f, terrain_component->GetPoint(position.x(), position.y()));
            }
        }
    }

    void EnvironmentEditor::BrushSizeChanged()
    {
        const QRadioButton *sender = qobject_cast<QRadioButton *>(QObject::sender());
        if(sender)
        {
            std::string object_name = sender->objectName().toStdString();
            if(object_name == "rad_button_small")
            {
                brush_size_ = Small;
            }
            else if(object_name == "rad_button_medium")
            {
                brush_size_ = Medium;
            }
            else if(object_name == "rad_button_large")
            {
                brush_size_ = Large;
            }
        }
    }

    void EnvironmentEditor::PaintActionChanged()
    {
        const QRadioButton *sender = qobject_cast<QRadioButton *>(QObject::sender());
        if(sender)
        {
            std::string object_name = sender->objectName().toStdString();
            if(object_name == "rad_button_flatten")
            {
                action_ = Flatten;
            }
            else if(object_name == "rad_button_raise")
            {
                action_ = Raise;
            }
            else if(object_name == "rad_button_lower")
            {
                action_ = Lower;
            }
            else if(object_name == "rad_button_smooth")
            {
                action_ = Smooth;
            }
            else if(object_name == "rad_button_roughen")
            {
                action_ = Roughen;
            }
            else if(object_name == "rad_button_revert")
            {
                action_ = Revert;
            }
        }
    }

    void EnvironmentEditor::TabWidgetChanged(int index)
    {
        if(index == 0) // Map tab
        {
            UpdateTerrain();
        }
        else if(index == 1) // Texture tab
        {
            if(!terrain_.get())
                return;

            QLineEdit *line_edit = 0;
            for(uint i = 0; i < cNumberOfTerrainTextures; i++)
            {
                //Get terrain texture asset ids so that we can request those image resources.
                RexTypes::RexAssetID terrain_id = terrain_->GetTerrainTextureID(i);

                UpdateTerrainTextureRanges();

                QString line_edit_name("texture_line_edit_" + QString("%1").arg(i + 1));

                // Check if terrain texture hasn't changed for last time, if not we dont need to request a new texture resource and we can continue on next texture.
                if(terrain_texture_id_list_[i] == terrain_id)
                    continue;

                terrain_texture_id_list_[i] = terrain_id;

                line_edit = editor_widget_->findChild<QLineEdit *>(line_edit_name);
                if(!line_edit)
                    continue;
                line_edit->setText(QString::fromStdString(terrain_id));

                terrain_texture_requests_[i] = RequestTerrainTexture(i);
            }
        }
    }

    void EnvironmentEditor::HandleResourceReady(Resource::Events::ResourceReady *res)
    {
        for(uint index = 0; index < terrain_texture_requests_.size(); index++)
        {
            if(terrain_texture_requests_[index] == res->tag_)
            {
                Foundation::TextureInterface *tex = dynamic_cast<Foundation::TextureInterface *>(res->resource_.get());
                if(!tex && tex->GetLevel() != 0)
                    return;

                // \todo Image should be able to create from raw data pointer, right now cant.
                //uint size = tex->GetWidth() * tex->GetHeight() * tex->GetComponents();
                QImage img = ConvertToQImage(*tex);//QImage::fromData(tex->GetData(), size);
                QLabel *texture_label = editor_widget_->findChild<QLabel *>("terrain_texture_label_" + QString("%1").arg(index + 1));
                texture_label->setPixmap(QPixmap::fromImage(img));
                //texture_label->show();
            }
        }
    }

    QImage EnvironmentEditor::ConvertToQImage(Foundation::TextureInterface &tex)
    {
        uint img_width        = tex.GetWidth(); 
        uint img_height       = tex.GetHeight(); 
        uint img_components   = tex.GetComponents();
        u8 *data              = tex.GetData();
        uint img_width_step   = img_width * img_components;
        QImage image;

        if(img_width > 0 && img_height > 0 && img_components > 0)
        {
            if(img_components == 3)// For RGB32
            {
                image = QImage(QSize(img_width, img_height), QImage::Format_RGB32);
                for(uint height = 0; height < img_height; height++)
                {
                    for(uint width = 0; width < img_width; width++)
                    {
                        u8 color[3];
                        for(uint comp = 0; comp < img_components; comp++)
                        {
                            uint index = (height % img_height) * (img_width_step) + ((width * img_components) % (img_width_step)) + comp;
                            color[comp] = data[index];
                        }
                        image.setPixel(width, height, qRgb(color[0], color[1], color[2]));
                    }
                }
            }
            else if(img_components == 4)// For ARGB32
            {
                image = QImage(QSize(img_width, img_height), QImage::Format_ARGB32);
                for(uint height = 0; height < img_height; height++)
                {
                    for(uint width = 0; width < img_width; width++)
                    {
                        u8 color[4];
                        for(uint comp = 0; comp < img_components; comp++)
                        {
                            uint index = (height % img_height) * (img_width_step) + ((width * img_components) % (img_width_step)) + comp;
                            color[comp] = data[index];
                        }
                        image.setPixel(width, height, qRgba(color[0], color[1], color[2], color[3]));
                    }
                }
            }
        }

        return image;
    }

    request_tag_t EnvironmentEditor::RequestTerrainTexture(uint index)
    {
        if(index > cNumberOfTerrainTextures) index = cNumberOfTerrainTextures;

        Foundation::ServiceManagerPtr service_manager = environment_module_->GetFramework()->GetServiceManager();
        if(service_manager)
        {
            if(service_manager->IsRegistered(Foundation::Service::ST_Texture))
            {
                boost::shared_ptr<Foundation::TextureServiceInterface> texture_service = 
                    service_manager->GetService<Foundation::TextureServiceInterface>(Foundation::Service::ST_Texture).lock();
                if(!texture_service)
                    return 0;

                // Request texture assets.
                return texture_service->RequestTexture(terrain_texture_id_list_[index]);
            }
        }
        return 0;
    }

    void EnvironmentEditor::UpdateGroundFog(float fogStart, float fogEnd, const QVector<float>& color)
    {
        // Adjust editor widget.

        // Ground fog
        QDoubleSpinBox* fog_ground_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_red_dSpinBox");
        QDoubleSpinBox* fog_ground_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_blue_dSpinBox");
        QDoubleSpinBox* fog_ground_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_green_dSpinBox");

        QDoubleSpinBox* fog_ground_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_start_distance_dSpinbox");
        QDoubleSpinBox* fog_ground_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_end_distance_dSpinbox");

        if ( fog_ground_red != 0 
            && fog_ground_blue != 0
            && fog_ground_green != 0
            && fog_ground_start_distance != 0
            && fog_ground_end_distance != 0)
        {
            fog_ground_red->setValue(color[0]);   
            fog_ground_blue->setValue(color[1]);
            fog_ground_green->setValue(color[2]);

            fog_ground_start_distance->setValue(fogStart);
            fog_ground_end_distance->setValue(fogEnd);
        }


    }
    
    void EnvironmentEditor::UpdateWaterFog(float fogStart, float fogEnd, const QVector<float>& color)
    {
        // Water fog
        QDoubleSpinBox* fog_water_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_red_dSpinBox");
        QDoubleSpinBox* fog_water_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_blue_dSpinBox");
        QDoubleSpinBox* fog_water_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_green_dSpinBox");

        QDoubleSpinBox* fog_water_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_start_distance_dSpinBox");
        QDoubleSpinBox* fog_water_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_end_distance_dSpinBox");

        if ( fog_water_red != 0 
            && fog_water_blue != 0
            && fog_water_green != 0
            && fog_water_start_distance != 0
            && fog_water_end_distance != 0)
        {
            fog_water_red->setValue(color[0]);   
            fog_water_blue->setValue(color[1]);
            fog_water_green->setValue(color[2]);

            fog_water_start_distance->setValue(fogStart);
            fog_water_end_distance->setValue(fogEnd);
        }

    }
    
    void EnvironmentEditor::SetGroundFog()
    {
        QDoubleSpinBox* fog_ground_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_red_dSpinBox");
        QDoubleSpinBox* fog_ground_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_blue_dSpinBox");
        QDoubleSpinBox* fog_ground_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_green_dSpinBox");

        if ( fog_ground_red != 0
             && fog_ground_blue != 0
             && fog_ground_green != 0)
        {
            QVector<float> color;
            color << fog_ground_red->value();
            color << fog_ground_blue->value();
            color << fog_ground_green->value();
            environment_->SetGroundFogColor(color);
        }

    }
    
    void EnvironmentEditor::SetWaterFog()
    {
        QDoubleSpinBox* fog_water_red = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_red_dSpinBox");
        QDoubleSpinBox* fog_water_blue = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_blue_dSpinBox");
        QDoubleSpinBox* fog_water_green = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_green_dSpinBox");

        if ( fog_water_red != 0
             && fog_water_blue != 0
             && fog_water_green != 0)
        {
            QVector<float> color;
            color << fog_water_red->value();
            color << fog_water_blue->value();
            color << fog_water_green->value();
            environment_->SetGroundFogColor(color);
        }
    }

    void EnvironmentEditor::SetGroundFogDistance()
    {
        QDoubleSpinBox* fog_ground_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_start_distance_dSpinBox");
        QDoubleSpinBox* fog_ground_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_ground_end_distance_dSpinBox");   

        if ( fog_ground_start_distance != 0 && fog_ground_end_distance != 0)
            environment_->SetGroundFogDistance(fog_ground_start_distance->value(), fog_ground_end_distance->value());

    }

    void EnvironmentEditor::SetWaterFogDistance()
    {
        QDoubleSpinBox* fog_water_start_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_start_distance_dSpinBox");
        QDoubleSpinBox* fog_water_end_distance = editor_widget_->findChild<QDoubleSpinBox* >("fog_water_end_distance_dSpinBox");   

        if ( fog_water_start_distance != 0 && fog_water_end_distance != 0)
            environment_->SetWaterFogDistance(fog_water_start_distance->value(), fog_water_end_distance->value());
    }

    void EnvironmentEditor::ToggleFogOverride()
    {
        if ( environment_->GetFogColorOverride() )
            environment_->SetFogColorOverride(false);
        else 
            environment_->SetFogColorOverride(true);

    }

    void EnvironmentEditor::UpdateAmbient()
    {
        // Sun direction
        QDoubleSpinBox* sun_direction_x = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_x");
        QDoubleSpinBox* sun_direction_y  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_y");
        QDoubleSpinBox* sun_direction_z  = editor_widget_->findChild<QDoubleSpinBox* >("sun_direction_z");
        
        // Sun color
        QDoubleSpinBox* sun_color_red = editor_widget_->findChild<QDoubleSpinBox* >("sun_color_red");
        QDoubleSpinBox* sun_color_blue  = editor_widget_->findChild<QDoubleSpinBox* >("sun_color_blue");
        QDoubleSpinBox* sun_color_green  = editor_widget_->findChild<QDoubleSpinBox* >("sun_color_green");
        QDoubleSpinBox* sun_color_alpha  = editor_widget_->findChild<QDoubleSpinBox* >("sun_color_alpha");

        // Ambient light
        QDoubleSpinBox* ambient_light_red = editor_widget_->findChild<QDoubleSpinBox* >("ambient_light_red");
        QDoubleSpinBox* ambient_light_blue = editor_widget_->findChild<QDoubleSpinBox* >("ambient_light_blue");
        QDoubleSpinBox* ambient_light_green = editor_widget_->findChild<QDoubleSpinBox* >("ambient_light_green");

         if ( sun_direction_x != 0
                 && sun_direction_y != 0
                 && sun_direction_z != 0
                 && sun_color_red != 0
                 && sun_color_blue != 0
                 && sun_color_green != 0
                 && sun_color_alpha != 0
                 && ambient_light_red != 0
                 && ambient_light_blue !=0
                 && ambient_light_green != 0)
                 
            {
                QVector<float> sun_direction(3);
                sun_direction[0] = static_cast<float>(sun_direction_x->value());
                sun_direction[1] = static_cast<float>(sun_direction_y->value());
                sun_direction[2] = static_cast<float>(sun_direction_z->value());
                environment_->SetSunDirection(sun_direction);


                QVector<float> sun_color(4);
                sun_color[0] = static_cast<float>(sun_color_red->value());
                sun_color[1] = static_cast<float>(sun_color_green->value());
                sun_color[2] = static_cast<float>(sun_color_blue->value());
                sun_color[3] = static_cast<float>(sun_color_alpha->value());
                environment_->SetSunColor(sun_color);

                QVector<float> ambient_light(3); 
                ambient_light[0] = static_cast<float>(ambient_light_red->value());
                ambient_light[1] = static_cast<float>(ambient_light_green->value());
                ambient_light[2] = static_cast<float>(ambient_light_blue->value());
                environment_->SetAmbientLight(ambient_light);
            }

    }
}
