#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "styles.h"
#include "millis.h"
#include <iostream>
#include <fstream>

using namespace std;
#include <QMessageBox>
#include <QProgressDialog>
#include <QDir>
#include <QFileDialog>
#include <unistd.h>

/*
 * TODO FUTURE
 * TODO - the table selection disappears when you click stuff and it should come back
 * BUG - on the initial load the percent symbol doesn't show over the sliders
 * TODO - you should be able to paste over an existing layer to overwrite it?

 Right now all Layers save all the data to file even if unneccessary
 ex: rgb Layers save all HSV values
 everything works right now so don't mess with it but eventually maybe change this to only save what is necessary


keyboard https://www.kdab.com/qt-input-method-virtual-keyboard/
encoder


 *
 */


QString PROJECT_PATH;

//RGB SLIDERS
#define RED 0
#define GREEN 1
#define BLUE 2
#define WHITE 3
#define BRIGHTNESS 4
//HSV SLIDERS
#define HUE 0
#define SAT 1
#define VAL 2
//GRADIENT SLIDERS
#define STARTHUE 0
#define STARTSAT 1
#define STARTVAL 2
#define ENDHUE 3
#define ENDSAT 4
#define ENDVAL 5
//STRIP TYPES
#define STRIP_WS2812B 0
#define STRIP_APA102 1
#define STRIP_NEOPIXEL_SKINNY 2
#define STRIP_NEOPIXEL 3
//CELL TYPES
#define CELL_TYPE_GROUP 0
#define CELL_TYPE_LAYER 1

#define SLD_CHANGE_THRESHOLD 4
enum ENC_MODE{
    SCROLL_HIGHLIGHT,
    CHANGE_VALUE
};
ENC_MODE encMode = SCROLL_HIGHLIGHT;

//MODES FOR A Layer
#define MODE_RGB 0
#define MODE_HSV 1
#define MODE_GRADIENT 2

#define NUM_TABS 4
#define TAB_RGB 0
#define TAB_HSV 1
#define TAB_GRADIENT 2
#define TAB_HOME 3

#define ARDUINO_MSG_DEL 75*1000

bool configDataSentToArduino[NUM_TABS];

//Save all global info
CONFIG config;
//Store the current project
Project currentProject;
Layer * layerOnClipboard = nullptr;
QStringList stripTypes;

QList<QList<QFrame*>> borders;
QList<QList<QSlider*>> sliders;
QList<QString> LayerNames;
USB_Comm * arduino;

QStringList strPositions;
QStringList comPORTS;
bool firstLoad = true;
bool isArduinoConnected = false;
const QString FILE_NAME = "config.txt";
const QString FILENAME_LayerS = "Layers.txt";
int curHighlight = 0, curSelection = -1;


Ui::MainWindow * ui2;

void changeBackgroundOfHSVSlider(QSlider * sld,int h, int s, int v){

    //The HUE color in QTCREATOR doesnt match what the LEDS actually look like so create an offset to make them match
    //On Arduino RED-> PURPLE is 0 to 235
    //On QTCreator RED -> PURPLE is 0 to 255
    //the value that comes in is in Arduino mode so convert
   // double zeroTo1 = h / 235.0;
    //h = zeroTo1 * 358;

    QString color = "hsva(" + QString::number(h) + "," + QString::number(s) + "," + QString::number(v) + ",100%)";

         QString style = "QSlider::groove:vertical { \
            width: 80px;  \
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 " + color + " stop:1 " + color + "); \
            margin: 2px 0; \
        } \
        \
        QSlider::handle:vertical { \
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f); \
            height: 25px; \
            margin: -2px 0; \
            border-radius: 8px; \
        }";
           sld->setStyleSheet(style);
}

void changeBackgroundOfRGBSlider(QSlider * sld, int R, int G, int B, double percent){
    //percent should be inverted
    percent = 1 - percent;
    double percOffset = percent + 0.02;
    QString color = "rgb(" +  QString::number(R) + "," + QString::number(G) + "," + QString::number(B) + ")";
    QString style = " QSlider::groove:vertical { \
        width: 80px; \
        background: qlineargradient(x1:0, y1: " + QString::number(percent) +", x2: 0, y2: " + QString::number(percOffset) +
        ", stop:0 rgb(255, 255,255), stop:1 " + color + "); \
        margin: 2px 0; \
    } \
    \
    QSlider::handle:vertical { \
        background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #b4b4b4, stop:1 #8f8f8f); \
        height: 25px; \
        margin: -2px 0;  \
        border-radius: 8px; \
    } ";

      sld->setStyleSheet(style);
}

void changeGradientStyleBasedOnSliders(Ui::MainWindow * ui){
  int sh = ui->sldStartHue->value();
  int ss = ui->sldStartSat->value();
  int sv = ui->sldStartVal->value();
  int eh = ui->sldEndHue->value();
  int es = ui->sldEndSat->value();
  int ev = ui->sldEndVal->value();
QString startHSV = "hsv(" + QString::number(sh) + "," + QString::number(ss) + "," + QString::number(sv) + ")";
QString endHSV = "hsv(" + QString::number(eh) + "," + QString::number(es) + "," + QString::number(ev) + ")";
QString style = " background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 " + startHSV + ", stop:1 " + endHSV + ");";
ui->frameGradient->setStyleSheet(style);
}

void highlightSlider(int sld){

    //TODO THIS DOES NOTHING
    /*if (config.tab == TAB_HOME ){
        cout << "Layers page no sliders" <<endl;
        return;
    }
    if (sld == -1){
        cout << "No slider to highlight!" <<endl;
        return;
    }
    if (sld == curHighlight){
        return;
    }

       //turn the last hightlight white
    borders[config.tab][curHighlight]->setStyleSheet(STYLE_WHITE_BORDER);

    //make them all white
    /*for(int i = 0; i < borders[config.tab].size(); i++){
       borders[config.tab][i]->setStyleSheet(STYLE_WHITE_BORDER);
    }*/


    //now highlight one of them
    //borders[config.tab][sld]->setStyleSheet(STYLE_HIGHLIGHT_BORDER);
   // curHighlight = sld;
}

void selectSlider(int sld){
/*
    //Layers page has no sliders
    if (sld == -1 || config.tab == TAB_HOME){
        cout << "No slider to highlight!" <<endl;
        return;
    }
    //slider should already be highlighted so just turn that one selected
    borders[config.tab][sld]->setStyleSheet(STYLE_SELECTED_BORDER);*/
}

void parseUSBCmd(string in){
    cout << "USB IN: " << in << endl;    
    if (in == "ready"){
        arduino->setConnected(true);
        cout << "Arduino is live" << endl;
    }
        else if (in == "lastload"){
           cout << " LOAD FINISHED"<<endl;
       } else if (in == "e"){

        if (encMode == SCROLL_HIGHLIGHT){
            //if we are scrolling now a click should select whatever is highlighted
            selectSlider(curHighlight);
            curSelection = curHighlight;
            encMode = CHANGE_VALUE;
        } else if (encMode == CHANGE_VALUE){
            highlightSlider(curSelection);
            curHighlight = curSelection;
            curSelection = -1; //deselect
            encMode = SCROLL_HIGHLIGHT;
        }

    } else if (in == "+"){
        if (encMode == SCROLL_HIGHLIGHT){
            int temp = curHighlight + 1;
            if (temp > sliders[config.tab].size()-1) { temp = 0;}
            highlightSlider(temp);
        } else if (encMode == CHANGE_VALUE){
            int curVal = sliders[config.tab][curSelection]->value();
            sliders[config.tab][curSelection]->setValue(curVal + 3);
        }
    } else if (in == "-"){
        if (encMode == SCROLL_HIGHLIGHT){
            int temp = curHighlight - 1;
            if (temp < 0) { temp = sliders[config.tab].size()-1;}
            highlightSlider(temp);
        } else if (encMode == CHANGE_VALUE){
            int curVal = sliders[config.tab][curSelection]->value();
            sliders[config.tab][curSelection]->setValue(curVal - 3);
        }
    }
}

void appendNum(QString& data, int n){
    //if the last char is the { then dont add a comma in front
    if (data.at(data.length()-1) == '{'){
         data.append(QString::number(n));
    } else {
       data.append("," + QString::number(n));
    }
}

void MainWindow::tableComboModeChanged(int typeIdx){
    QComboBox * comboBox = dynamic_cast<QComboBox *>(sender());

    if(comboBox)
    {
        int row = comboBox->itemData(0).toInt();
        int cellType = getRowObjectFromTable(row);
        if (cellType == CELL_TYPE_LAYER){
            Layer * l = getLayerFromTable(row);
            l->type = typeIdx;
            saveProjectToFile();
            sendLayerInfo(l);
            refreshProjectTable();
        }
        /*cout << "Combo Box Changed on row " << row << endl;
        Layers[row].type = idx;
        saveLayersToFile();
        //show Layers because this repopulates the combo boxes that hold each Layer type
        //and a Layer may have switch from RGB to HSV
        showLayers();*/
    }
}

//TODO when starting the app with Group1 and 3 layers layer 1 was not in the RGB combo drop down, but the other 2 groups were

void MainWindow::tableComboStatusChanged(int statusIdx){
    QComboBox * comboBox = dynamic_cast<QComboBox *>(sender());

    if(comboBox)
    {
        int row = comboBox->itemData(0).toInt();
        //change the status of the group or layer in the project
        int cellType = getRowObjectFromTable(row);
        if (cellType == CELL_TYPE_GROUP){
            Group * g = getGroupFromTable(row);
            g->status = statusIdx;            
            saveProjectToFile();
            //TODO if the group status changed then you need to change all sub layers
            //status as well if it's not ACTIVE - active just lets the layer status work freely
            for(size_t i = 0; i < g->layers.size(); i++){
                g->layers[i].status = statusIdx;
                sendLayerInfo(&g->layers[i]);
                //TODO make delay time between groups a define at the top
                usleep(50 * 1000);
            }
            refreshProjectTable();
        } else if (cellType == CELL_TYPE_LAYER){
            Layer * l = getLayerFromTable(row);
            l->status = statusIdx;
            saveProjectToFile();
            sendLayerInfo(l);
            //we need to refresh the project because changin the status may add or remove the layer from
            //a drop down combo box
            refreshProjectTable();
        }


    }
}
//create a combo box for row of Layers
QComboBox * MainWindow::createLayerModeCombo(int row,int idx){
    QComboBox * cmbMode = new QComboBox();
    cmbMode->blockSignals(true);
    cmbMode->addItem("RGB"); cmbMode->addItem("HSV"); cmbMode->addItem("Gradient");

    cmbMode->setCurrentIndex(idx);

    //set the row of the combo box so it knows in the future
    cmbMode->setItemData(0,row);

    cmbMode->setStyleSheet("QComboBox QAbstractItemView { selection-background-color: rgb(0,0,127); selection-color: rgb(0, 0, 0); }");
    //connect the combo box to a lambda right here for the index changed
    connect(cmbMode, SIGNAL(currentIndexChanged(int)), this, SLOT(tableComboModeChanged(int)));
    //connect(cmbMode, &QComboBox::currentIndexChanged, [row](){cout << "CMB ROW: " << row << " INDEX: " << idx << endl;});

    cmbMode->blockSignals(false);
    return cmbMode;
}

QComboBox * MainWindow::createLayerStatusCombo(int row,int idx){
    QComboBox * cmdStatus = new QComboBox();
    cmdStatus->blockSignals(true);
    cmdStatus->addItem("active"); cmdStatus->addItem("inactive"); cmdStatus->addItem("off");

    cmdStatus->setCurrentIndex(idx);

    //set the row of the combo box so it knows in the future
    cmdStatus->setItemData(0,row);

    cmdStatus->setStyleSheet("QComboBox QAbstractItemView { selection-background-color: rgb(0,0,127); selection-color: rgb(0, 0, 0); }");
    //connect the combo box to a lambda right here for the index changed
    connect(cmdStatus, SIGNAL(currentIndexChanged(int)), this, SLOT(tableComboStatusChanged(int)));
    //connect(cmdStatus, &QComboBox::currentIndexChanged, [row](){cout << "CMB ROW: " << row << " INDEX: " << idx << endl;});
    cmdStatus->blockSignals(false);
    return cmdStatus;
}

QPushButton * MainWindow::createLayerEditButton(int row){
    QPushButton * btn = new QPushButton("EDIT");
    //make text black and background gray
    btn->setStyleSheet("color: black; background: rgb(170,170,170)");

    //connect the button to a lambda right here for the click
    connect(btn, &QPushButton::clicked, [this,row](){
        //use the row in the table to get the layer
        Layer * l = getLayerFromTable(row);

        //if the layer is not active tell user
        if (l->status == INACTIVE || l->status == OFF){
            displayStandardMessageBox("This layer is either turned OFF or INACTIVE.\nYou will not be able to see the changes you are making.\nPlease make this layer ACTIVE if you wish to edit it.");
            return;
        }

        //go to the correct page based on the Layer
        if (l->type == MODE_RGB){
            ui->tabWidget->setCurrentIndex(TAB_RGB);
            //select the right Layer in the combo box
            for(int i = 0; i < ui->cmbRGB_Layers->count(); i++){
                cout << ui->cmbRGB_Layers->itemText(i).toStdString() << endl;
                //the item data should match the row in the table that was clicked
                if (ui->cmbRGB_Layers->itemData(i) == row){
                    ui->cmbRGB_Layers->setCurrentIndex(i);
                    break;
                }
            }
        } else if (l->type == MODE_HSV){
            ui->tabWidget->setCurrentIndex(TAB_HSV);
            //select the right Layer in the combo box
            for(int i = 0; i < ui->cmbHSV_Layers->count(); i++){
                cout << ui->cmbHSV_Layers->itemText(i).toStdString() << endl;
                if (ui->cmbHSV_Layers->itemData(i) == row){
                    ui->cmbHSV_Layers->setCurrentIndex(i);
                    break;
                }
            }
        } else if (l->type == MODE_GRADIENT){
            ui->tabWidget->setCurrentIndex(TAB_GRADIENT);
            //select the right Layer in the combo box
            for(int i = 0; i < ui->cmbGradient_Layers->count(); i++){
                cout << ui->cmbGradient_Layers->itemText(i).toStdString() << endl;
                if (ui->cmbGradient_Layers->itemData(i) == row){
                    ui->cmbGradient_Layers->setCurrentIndex(i);
                    break;
                }
            }
        }

    //load the sliders based on the Layer
    loadLayerToSliders( getLayerFromTable(row));
    });

    return btn;
}


//define the columns - the first column will be the group name and layers
//are indented in 1, so leave it blank
#define COL_GROUP_NAME 0
#define COL_LAYER_NAME 1
#define COL_STARTLED 2
#define COL_STOPLED 3
#define COL_MODE 4
#define COL_EDITBTN 5
#define COL_STATUS 6

void testProjectStructure(){

    //make 3 layers
   /* Layer l1 = {"Layer 1",1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
    Layer l2 = {"Layer 2",2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
    Layer l3 = {"Layer 3",3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21};

    //put them in a group
    Group g;
    g.name = "Group 1";
    g.addLayer(l1);
    g.addLayer(l2);
    g.addLayer(l3);

    //put them in structure
    Project p;
    p.name = "Project 1";
    p.addGroup(g);

    ofstream outfile("testProject.txt");
    outfile << p.toString();
    outfile.close();*/

    //read it in
    ifstream inFile("testProject.txt");
    Project p;
    p.readProjectFromFile(inFile);
    inFile.close();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui2 = ui;

    PROJECT_PATH  = QDir::currentPath() + "/projects/";
    cout << "PROJECT DIRECTORY" << PROJECT_PATH.toStdString() << endl;

    //mark that no page data has been sent to arduino yet
    for(int i = 0 ; i < NUM_TABS; i++){
        configDataSentToArduino[i] = false;
    }

    string port = "/dev/ttyACM0";
    arduino = new USB_Comm(port);
    arduino->setParseFunc(parseUSBCmd);
    //arduino->setParseByteFunc(parseByteFunct);
    usleep(2000*1000);

    while(!arduino->isConnected()){
        arduino->sendMsg("init");
        //usleep(500*1000);
    }

    //LOAD CONFIG TRIES TO CHANGE THE DROPDOWN SO POPULATE THIS FIRST
    //setup the strips dropdown
    stripTypes.append("WS2812B");
    stripTypes.append("APA102");
    stripTypes.append("NEOPIXEL SKINNY");
    ui->cmbStripType->setModel( new QStringListModel(stripTypes) );

    ui->btnPaste->setEnabled(false);
    ui->btnCopyTo->setEnabled(false);

    loadConfigFromFile();

    //A 2D array of the boarders and sliders is used to highlight certain sliders and also to change their value
    //Each row is a mode, and the column is a list of pointers to the objects on the page for that mode
    //put borders on list to make highlight/selection easier
    QList<QFrame*> tempBorders;
    tempBorders.push_back(ui->brdRed);
    tempBorders.push_back(ui->brdGreen);
    tempBorders.push_back(ui->brdBlue);
    //tempBorders.push_back(ui->brdTungsten);
    //tempBorders.push_back(ui->brdDaylight);
    tempBorders.push_back(ui->brdWhite);
    tempBorders.push_back(ui->brdBrightness);
    borders.push_back(tempBorders);
    QList<QSlider *> tempSliders;
    //put sliders on list to make changing values easier
    tempSliders.push_back(ui->sldRed);
    tempSliders.push_back(ui->sldGreen);
    tempSliders.push_back(ui->sldBlue);
    //tempSliders.push_back(ui->sldTungsten);
    //tempSliders.push_back(ui->sldDaylight);
    tempSliders.push_back(ui->sldWhite);
    tempSliders.push_back(ui->sldBrightness);
    sliders.push_back(tempSliders);
    //setup HSV page
    tempBorders.clear();
    tempBorders.push_back(ui->brdHue);
    tempBorders.push_back(ui->brdSat);
    tempBorders.push_back(ui->brdVal);
    borders.push_back(tempBorders);
    tempSliders.clear();
    tempSliders.push_back(ui->sldHue);
    tempSliders.push_back(ui->sldSat);
    tempSliders.push_back(ui->sldVal);
    sliders.push_back(tempSliders);
    //setup gradient page
    tempBorders.clear();
    tempBorders.push_back(ui->brdStartHue);
    tempBorders.push_back(ui->brdStartSat);
    tempBorders.push_back(ui->brdStartVal);
    tempBorders.push_back(ui->brdEndHue);
    tempBorders.push_back(ui->brdEndSat);
    tempBorders.push_back(ui->brdEndVal);
    borders.push_back(tempBorders);
    tempSliders.clear();
    tempSliders.push_back(ui->sldStartHue);
    tempSliders.push_back(ui->sldStartSat);
    tempSliders.push_back(ui->sldStartVal);
    tempSliders.push_back(ui->sldEndHue);
    tempSliders.push_back(ui->sldEndSat);
    tempSliders.push_back(ui->sldEndVal);
    sliders.push_back(tempSliders);

    highlightSlider(curHighlight);

   //set the tab to the last tab we were on
    ui->tabWidget->setCurrentIndex(static_cast<int>(config.tab) );
\
    //set header list
    ui->tblGroups->setHorizontalHeaderItem(COL_GROUP_NAME, new QTableWidgetItem("Group Name"));
    ui->tblGroups->setHorizontalHeaderItem(COL_LAYER_NAME, new QTableWidgetItem("Layer Name"));
    ui->tblGroups->setHorizontalHeaderItem(COL_STARTLED, new QTableWidgetItem("Start LED"));
    ui->tblGroups->setHorizontalHeaderItem(COL_STOPLED, new QTableWidgetItem("Stop LED"));
    ui->tblGroups->setHorizontalHeaderItem(COL_MODE, new QTableWidgetItem("Type"));
    ui->tblGroups->setHorizontalHeaderItem(COL_EDITBTN, new QTableWidgetItem("Edit"));
    ui->tblGroups->setHorizontalHeaderItem(COL_STATUS, new QTableWidgetItem("Active"));

    //set column widths
    ui->tblGroups->setColumnWidth(COL_GROUP_NAME, 120);
    ui->tblGroups->setColumnWidth(COL_LAYER_NAME, 120);
    ui->tblGroups->setColumnWidth(COL_STARTLED, 90);
    ui->tblGroups->setColumnWidth(COL_STOPLED, 90);
    ui->tblGroups->setColumnWidth(COL_EDITBTN, 30);
    ui->tblGroups->setColumnWidth(COL_STATUS, 100);

    //on start you shouldnt be able to delete group or layer
    ui->btnDeleteGroup->setEnabled(false);
    ui->btnDeleteLayer->setEnabled(false);

    //load the last project if it has one

    //connect all of the master sliders to the function
    connect(ui->sldMasterRGB, &QSlider::valueChanged, this, &MainWindow::on_sldMasterBrightness);
    connect(ui->sldMasterHSV, &QSlider::valueChanged, this, &MainWindow::on_sldMasterBrightness);
    connect(ui->sldMasterGradient, &QSlider::valueChanged, this, &MainWindow::on_sldMasterBrightness);
    if (config.lastOpenProject.length() > 0){
        loadProjectFromFile(config.lastOpenProject, true);
    }
    usleep(ARDUINO_MSG_DEL);
    sendStartUpData(true); //send true for first load


}


void MainWindow::sendStartUpData(bool firstLoad){

    //connect
    while(!arduino->isConnected()){
        arduino->sendMsg("init");
        //usleep(500*1000);
    }
    //send number of LEDS
    sendArduinoCmd("nl:" + QString::number(config.numLEDs));
    usleep(100*1000);

    //send current mode data
    //when you change the strip on the Layers tabs it will have the Layers as the mode which doesnt send any data
    //if this is not the first load we should send the RGB mode info by default
    if (firstLoad){
        //send current tab that we're on
       // sendArduinoCmd("m:" + QString::number(static_cast<int>(config.tab)));

        //if we're on the Layers page send the RGB data
        if (config.tab == TAB_HOME){
            sendTabData(TAB_RGB);
        } else {
            sendTabData(config.tab);
        }
    } else {
        //send current tab that we're on
        //sendArduinoCmd("m:" + QString::number(MODE_RGB));
        sendTabData(TAB_RGB);
    }
    usleep(100*1000);
    sendAllLayerInfo();



}

void MainWindow::sendAllLayerInfo(){

    //TODO : This could take some time so show progress box while the layers load
    //int numLayers = currentProject.getTotalLayers();
    //QProgressDialog progress("Updating LED Strip...", "Cancel", 0, numLayers, this);
   // progress.setWindowModality(Qt::WindowModal);
   // progress.show();
    //ui->widProgressPop->setVisible(true);
   // ui->tblGroups->setEnabled(false);
   // ui->lblProgress->setVisible(true);
   // ui->lblProgress->setText("Updating LED Strip\nLayer 1 of " + QString::number(numLayers));

    //send the Layer info to the arduino
    int layerCo = 0;
    for(size_t g = 0; g < currentProject.groups.size(); g++){
        for(size_t l = 0 ; l < currentProject.groups[g].layers.size(); l++){
            sendLayerInfo(&currentProject.groups[g].layers[l]);
            usleep(ARDUINO_MSG_DEL);
            layerCo++;
            //ui->lblProgress->setText("Updating LED Strip\nLayer "+ QString::number(layerCo) +" of " + QString::number(numLayers));
        }
     }

   // ui->lblProgress->setVisible(false);
    //ui->tblGroups->setEnabled(true);

}

//the mode data is based on the tab that we're on
void MainWindow::sendTabData(int idx){

    if (idx == TAB_RGB){
        //rgb
       /* sendArduinoCmd("r:" + QString::number(config.r));
        sendArduinoCmd("g:" + QString::number(config.g));
        sendArduinoCmd("b:" + QString::number(config.b));
        sendArduinoCmd("d:" + QString::number(config.d));
        sendArduinoCmd("t:" + QString::number(config.t));
        sendArduinoCmd("B:" + QString::number(config.B));*/
        QString msg = "l0{";
        appendNum(msg, config.r);
        appendNum(msg,config.g);
        appendNum(msg,config.b);
        appendNum(msg,config.d);
        appendNum(msg,config.t);
        appendNum(msg,config.w);
        appendNum(msg,config.B);
        msg += "}";
        sendArduinoCmd(msg);
        loadSliders();
        highlightSlider(0);
    } else if (idx == TAB_HSV){
        //THIS SHOULD BE CONVERTED FOR THE SINGLE MESSAGE TO LOAD BUT HOLD OFF BECAUSE IT MIGHT CHANGE TO JUST BYTES ANYWAY
        //AND NOW THAT THERE"S ACK THIS SEEMS TO BE WORKING BETTER
        //hsv
        QString msg = "l1{";
        appendNum(msg, config.h);
        appendNum(msg,config.s);
        appendNum(msg,config.v);
        msg += "}";
        sendArduinoCmd(msg);
        loadSliders();
        highlightSlider(0);
    } else if (idx == TAB_GRADIENT){
        //gradient
        QString msg = "l2{";
        appendNum(msg, config.sh);
        appendNum(msg,config.ss);
        appendNum(msg,config.sv);
        appendNum(msg, config.eh);
        appendNum(msg,config.es);
        appendNum(msg,config.ev);
        msg += "}";
        sendArduinoCmd(msg);
        loadSliders();
        highlightSlider(0);
    }
    //mark that we sent data for that page
    configDataSentToArduino[idx] = true;
}

int getPercentFromVal(int val, int max){
    double percent = (double) val / max;
    return percent * 100;
}

//call this after everything has been drawn
void MainWindow::loadSliders(){

    if (config.tab == TAB_RGB){
        //move all sliders down and show no background
        changeBackgroundOfRGBSlider(ui->sldRed, 255,0,0, config.r / 255.0);
        ui->lblRedPercent->setText( QString::number( getPercentFromVal(config.r,255) ) + "%");
        changeBackgroundOfRGBSlider(ui->sldGreen,0,255,0, config.g / 255.0);
        ui->lblGreenPercent->setText( QString::number( getPercentFromVal(config.g,255) ) + "%" );
        changeBackgroundOfRGBSlider(ui->sldBlue, 0,0,255, config.b / 255.0);
        ui->lblBluePercent->setText( QString::number( getPercentFromVal(config.b,255) ) + "%" );
        //changeBackgroundOfRGBSlider(ui->sldDaylight, 198, 243, 255, config.t / 255.0);
        //changeBackgroundOfRGBSlider(ui->sldTungsten, 255,212,125 , config.d / 255.0);
        changeBackgroundOfRGBSlider(ui->sldWhite, 255,255,255 , config.w / 255.0);
        ui->lblWhitePercent->setText( QString::number( getPercentFromVal(config.w,255) ) + "%" );
        changeBackgroundOfRGBSlider(ui->sldBrightness,194,194,194, config.B / 255.0);
        ui->lblBrightnessPercent->setText( QString::number( getPercentFromVal(config.B,255) ) + "%");

        setSliderSilent(ui->sldRed, config.r);
        setSliderSilent(ui->sldGreen,config.g);
        setSliderSilent(ui->sldBlue,config.b);
        //setSliderSilent(ui->sldDaylight,config.d);
        //setSliderSilent(ui->sldTungsten,config.t);
        setSliderSilent(ui->sldWhite,config.w);
        setSliderSilent(ui->sldBrightness,config.B);
    } else  if (config.tab  == TAB_HSV){
        //hsv
        //QTCreator is 0 to 359 but arduino is 0 to 254
        double zeroTo1 = config.h / 255.0;
        int value = (int)(zeroTo1 * 359.0);
        setSliderSilent(ui->sldHue,value);
        setSliderSilent(ui->sldSat,config.s);
        setSliderSilent(ui->sldVal,config.v);
        changeBackgroundOfHSVSlider(ui->sldHue, value,180,255);
        changeBackgroundOfHSVSlider(ui->sldSat, value,config.s,255);
        changeBackgroundOfHSVSlider(ui->sldVal,value,config.s,config.v);
        ui->lblHuePercent->setText( QString::number( config.h));
        ui->lblSatPercent->setText( QString::number( getPercentFromVal(config.s,255) ) + "%");
        ui->lblValPercent->setText( QString::number( getPercentFromVal(config.v,255) ) + "%");

    } else  if (config.tab  == TAB_GRADIENT){

        //gradient
        //convert the hue
        double zeroTo1 = config.sh / 255.0;
        int value = (int)(zeroTo1 * 359.0);
        setSliderSilent(ui->sldStartHue,value);
        setSliderSilent(ui->sldStartSat,config.ss);
        setSliderSilent(ui->sldStartVal,config.sv);
        changeBackgroundOfHSVSlider(ui->sldStartHue, value,180,255);
        changeBackgroundOfHSVSlider(ui->sldStartSat, value,config.ss,255);
        changeBackgroundOfHSVSlider(ui->sldStartVal, value, config.ss, config.sv);
        ui->lblStartHuePercent->setText( QString::number( config.sh));
        ui->lblStartSatPercent->setText( QString::number( getPercentFromVal(config.ss,255) ) + "%");
        ui->lblStartValPercent->setText( QString::number( getPercentFromVal(config.sv,255) ) + "%");

        zeroTo1 = config.eh / 255.0;
        value = (int)(zeroTo1 * 359.0);
        setSliderSilent(ui->sldEndHue,value);
        setSliderSilent(ui->sldEndSat,config.es);
        setSliderSilent(ui->sldEndVal,config.sv);
        changeBackgroundOfHSVSlider(ui->sldEndHue, value,180,255);
        changeBackgroundOfHSVSlider(ui->sldEndSat, value,config.es,255);
        changeBackgroundOfHSVSlider(ui->sldEndVal,value,config.es,config.ev);
        ui->lblEndHuePercent->setText( QString::number( config.eh));
        ui->lblEndSatPercent->setText( QString::number( getPercentFromVal(config.es,255) ) + "%");
        ui->lblEndValPercent->setText( QString::number( getPercentFromVal(config.ev,255) ) + "%");

        changeGradientStyleBasedOnSliders(ui);
    }

   // sendArduinoCmd("lastload");
}

/*
void MainWindow::parseArduinoCmd(string in){

     cout << "USB IN: " << in << endl;
     ui->lblIncMsg->setText(QString::fromStdString(in));

        if (in == "ready"){
            isArduinoConnected = true;
           // loadSliders();
        } else if (in == "lastload"){
            cout << " LOAD FINISHED"<<endl;
        } else if (in == "enc"){

         if (encMode == SCROLL_HIGHLIGHT){
             //if we are scrolling now a click should select whatever is highlighted
             selectSlider(curHighlight);
             curSelection = curHighlight;
             encMode = CHANGE_VALUE;
         } else if (encMode == CHANGE_VALUE){
             highlightSlider(curSelection);
             curHighlight = curSelection;
             curSelection = -1; //deselect
             encMode = SCROLL_HIGHLIGHT;
         }

     } else if (in == "enc+"){
         if (encMode == SCROLL_HIGHLIGHT){
             curHighlight++;
             if (curHighlight > sliders[config.tab].size()-1) { curHighlight = 0;}
             highlightSlider(curHighlight);
         } else if (encMode == CHANGE_VALUE){
             int curVal = sliders[config.tab][curSelection]->value();
             sliders[config.tab][curSelection]->setValue(curVal + 3);
         }
     } else if (in == "enc-"){
         if (encMode == SCROLL_HIGHLIGHT){
             curHighlight--;
             if (curHighlight < 0) { curHighlight = sliders[config.tab].size()-1;}
             highlightSlider(curHighlight);
         } else if (encMode == CHANGE_VALUE){
             int curVal = sliders[config.tab][curSelection]->value();
             sliders[config.tab][curSelection]->setValue(curVal - 3);
         }
     }

     if (this->cursor() != Qt::ArrowCursor){
        this->setCursor(Qt::ArrowCursor);
     }


}*/

void MainWindow::sendArduinoCmd(QString in){

   //when the sliders move it can cause a lot of messages to send
    //only send messages once every 30 ms
   /* while (getMillis() - lastArduinoSend < 25 ){
        ;;
    }
  */
  arduino->sendMsg(in.toStdString());
  //usleep(20*1000);
}

void MainWindow::on_btnClose_clicked()
{

    arduino->close();
    MainWindow::close();
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::getLayerIdx(Layer * layer){


    int layerCo = 0;
    //find the Layer in the list, assume all Layers are unique names
    for(size_t g = 0; g < currentProject.groups.size(); g++){
        for(size_t l = 0 ; l < currentProject.groups[g].layers.size(); l++){
            if (layer == &currentProject.groups[g].layers[l]){
                return layerCo;
            }
            layerCo++;
        }
    }

    cout << "Could not find the Layer to get the index: " << layer->name.toStdString() <<endl;
    return -1;

}
/*
 * This function processes a change from one of the sliders, saves it locally and also sends it to arduino
 * Params:
 * value - the value of the slider
 * *configVal  - if it is master mode we need a pointer to the config value so we can change it to the slider val
 * *layerVal - if its a layer we're changin we need a pointer to the layer value so we can change it to the slider val
 * layerIdx - the index of the layer within the context of all layers. the arduino just knows layers, not groups
 * arduinoCmd - the cmd value to send to arduino
 * hsvConvert - if it is an hsv slider you need to conver teh 0-360 to 0-180
*/
void MainWindow::processSliderChange(int value, int * configVal, int * LayerVal, int layerIdx, Layer * l, QString arduinoCmd, bool hsvConvert){

    if (hsvConvert){
        //QTCreator is 0 to 359 but arduino is 0 to 254
        double zeroTo1 = value / 359.0;
        value = (int)(zeroTo1 * 254);
    }

    //if master Layer check if it changed enough
    if (layerIdx == -1){
        //check if we've moved the slider enough to send the new data
        if (abs(*configVal - value) > SLD_CHANGE_THRESHOLD || *configVal > 252 )
        {
                *configVal = value;
                saveConfigToFile();
                QString msg = arduinoCmd + QString::number(value);
                sendArduinoCmd(msg);
        }

    }
    //check if the slider changed this much for this Layer
    else {

        if (abs(*LayerVal - value) > SLD_CHANGE_THRESHOLD || *LayerVal > 252)
        {
            *LayerVal = value;
            //saveLayersToFile();
            saveProjectToFile();
            sendLayerInfo(l);
        }
    }
}

void MainWindow::on_sldRed_valueChanged(int value)
{
    //turn the value into a valid Arduino command
    //r:100
    //cout << "RED CHANGED" << value << endl;

    changeBackgroundOfRGBSlider(ui->sldRed, 200, 0, 0 , value/255.0);
    if (curSelection != RED){
       curSelection = RED;
       curHighlight = RED;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbRGB_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.r, &l->r, layerIdx, l, "r:");


    double percent = (double) value / 255;
    int p = percent * 100;
    ui->lblRedPercent->setText( QString::number(p) + "%");
}

void MainWindow::on_sldGreen_valueChanged(int value)
{
    //turn the value into a valid Arduino command
    //r:100

    changeBackgroundOfRGBSlider(ui->sldGreen, 0, 200, 0 , value/255.0);
    if (curSelection != GREEN){
       curSelection = GREEN;
       curHighlight = GREEN;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbRGB_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.g, &l->g, layerIdx, l, "g:");

    double percent = (double) value / 255;
    int p = percent * 100;
    ui->lblGreenPercent->setText( QString::number(p) + "%");
}

void MainWindow::on_sldBlue_valueChanged(int value)
{
    //turn the value into a valid Arduino command
    //r:100

     changeBackgroundOfRGBSlider(ui->sldBlue, 0, 0, 200, value/255.0);
    if (curSelection != BLUE){
       curSelection = BLUE;
       curHighlight = BLUE;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbRGB_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.b, &l->b, layerIdx, l , "b:");
    //if master Layer check if it changed enough

    double percent = (double) value / 255;
    int p = percent * 100;
    ui->lblBluePercent->setText( QString::number(p) + "%");

}
/*
void MainWindow::on_sldTungsten_valueChanged(int value)
{
    //turn the value into a valid Arduino command
    //r:100

    changeBackgroundOfRGBSlider(ui->sldTungsten, 255,212,125 , value/255.0);
    if (curSelection != TUNGSTEN){
       curSelection = TUNGSTEN;
       curHighlight = TUNGSTEN;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbRGB_Layers->currentData().toInt();
    if (layerIdx == -1){
        return;
    }
    processSliderChange(value, &config.d, &getLayerFromName(LayersRGB[LayerIdx])->d, LayerIdx == 0 ? "MASTER" : LayersRGB[LayerIdx] , "d:");

}*/
/*
void MainWindow::on_sldDaylight_valueChanged(int value)
{
    //turn the value into a valid Arduino command
    //r:100

    changeBackgroundOfRGBSlider(ui->sldDaylight, 198, 243, 255 , value/255.0);
    if (curSelection != DAYLIGHT){
       curSelection = DAYLIGHT;
       curHighlight = DAYLIGHT;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    //on load this can get called before the view is loaded which gives -1 at the index
    int LayerIdx = ui->cmbRGB_Layers->currentIndex();
    if (LayerIdx == -1){
        return;
    }
    processSliderChange(value, &config.t, &getLayerFromName(LayersRGB[LayerIdx])->t, LayerIdx == 0 ? "MASTER" : LayersRGB[LayerIdx] , "t:");

}
*/

void MainWindow::on_sldWhite_valueChanged(int value)
{
    //White doesn't have a background
    //changeBackgroundOfRGBSlider(ui->sldBlue, 0, 0, 200, value/255.0);
   if (curSelection != BLUE){
      curSelection = BLUE;
      curHighlight = BLUE;
      highlightSlider(curHighlight);
      selectSlider(curSelection);
  }

   //on load this can get called before the view is loaded which gives -1 at the index
   int layerIdx = ui->cmbRGB_Layers->currentData().toInt();
   Layer * l = nullptr;
   //if not master get the layer info
   if (layerIdx != -1){
       l = getLayerFromTable(layerIdx);
       //the arduino needs the overall layer idx not it's layer in the table
       layerIdx = getLayerIdx(l);
   }
   processSliderChange(value, &config.w, &l->w, layerIdx, l, "w:");
   double percent = (double) value / 255;
   int p = percent * 100;
   ui->lblWhitePercent->setText( QString::number(p) + "%");
}

void MainWindow::on_sldBrightness_valueChanged(int value)
{
    //turn the value into a valid Arduino command
    //r:100

    changeBackgroundOfRGBSlider(ui->sldBrightness, 194,194,194, value/255.0);
    if (curSelection != BRIGHTNESS){
       curSelection = BRIGHTNESS;
       curHighlight = BRIGHTNESS;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbRGB_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.B, &l->B,layerIdx, l , "B:");

    double percent = (double) value / 255;
    int p = percent * 100;
    ui->lblBrightnessPercent->setText( QString::number(p) + "%");

}

void MainWindow::on_sldHue_valueChanged(int value)
{
    if (curSelection != HUE){
       curSelection = HUE;
       curHighlight = HUE;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    changeBackgroundOfHSVSlider(ui->sldHue, value,180,255);
    changeBackgroundOfHSVSlider(ui->sldSat, value,ui->sldSat->value(),255);
    changeBackgroundOfHSVSlider(ui->sldVal, value,ui->sldSat->value(),ui->sldVal->value());

    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbHSV_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.h, &l->sh,layerIdx, l, "h:", true);
    ui->lblHuePercent->setText( QString::number(value) );

}

void MainWindow::on_sldSat_valueChanged(int value)
{
    if (curSelection != SAT){
       curSelection = SAT;
       curHighlight = SAT;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    changeBackgroundOfHSVSlider(ui->sldSat, ui->sldHue->value(),value,255);
    changeBackgroundOfHSVSlider(ui->sldVal, ui->sldHue->value(),value,ui->sldVal->value());

    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbHSV_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.s, &l->ss, layerIdx, l, "s:");
    ui->lblSatPercent->setText( QString::number(getPercentFromVal(value,255)) + "%");

}

void MainWindow::on_sldVal_valueChanged(int value)
{
    if (curSelection != VAL){
       curSelection = VAL;
       curHighlight = VAL;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    changeBackgroundOfHSVSlider(ui->sldVal, ui->sldHue->value(),ui->sldSat->value(),value);

    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbHSV_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.v, &l->sv, layerIdx, l, "v:");
    ui->lblValPercent->setText( QString::number(getPercentFromVal(value,255)) + "%");
}

void MainWindow::on_sldStartHue_valueChanged(int value)
{

    if (curSelection != STARTHUE){
       curSelection = STARTHUE;
       curHighlight = STARTHUE;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    changeBackgroundOfHSVSlider(ui->sldStartHue, value,180,255);
    changeBackgroundOfHSVSlider(ui->sldStartSat, value,ui->sldStartSat->value(),255);
    changeBackgroundOfHSVSlider(ui->sldStartVal, value,ui->sldStartSat->value(),ui->sldStartVal->value());
    changeGradientStyleBasedOnSliders(ui);


    //on load this can get called before the view is loaded which gives -1 at the index
    int layerIdx = ui->cmbGradient_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.sh, &l->sh, layerIdx, l, "sh:", true);
    ui->lblStartHuePercent->setText( QString::number(value));
}

void MainWindow::on_sldStartSat_valueChanged(int value)
{

    if (curSelection != STARTSAT){
       curSelection = STARTSAT;
       curHighlight = STARTSAT;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    changeBackgroundOfHSVSlider(ui->sldStartSat, ui->sldStartHue->value(),value,255);
    changeBackgroundOfHSVSlider(ui->sldStartVal, ui->sldStartHue->value(),value,ui->sldStartVal->value());
    changeGradientStyleBasedOnSliders(ui);

    int layerIdx = ui->cmbGradient_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.ss, &l->ss, layerIdx, l, "ss:");
    ui->lblStartSatPercent->setText( QString::number(getPercentFromVal(value,255)) + "%");
}

void MainWindow::on_sldStartVal_valueChanged(int value)
{
    if (curSelection != STARTVAL){
       curSelection = STARTVAL;
       curHighlight = STARTVAL;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    changeBackgroundOfHSVSlider(ui->sldStartVal, ui->sldStartHue->value(),ui->sldStartSat->value(),value);
    changeGradientStyleBasedOnSliders(ui);

    int layerIdx = ui->cmbGradient_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.sv, &l->sv, layerIdx, l, "sv:");
    ui->lblStartValPercent->setText( QString::number(getPercentFromVal(value,255)) + "%");
}

void MainWindow::on_sldEndHue_valueChanged(int value)
{
    if (curSelection != ENDHUE){
       curSelection = ENDHUE;
       curHighlight = ENDHUE;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }

    changeBackgroundOfHSVSlider(ui->sldEndHue, value,180,255);
    changeBackgroundOfHSVSlider(ui->sldEndSat, value,ui->sldEndSat->value(),255);
    changeBackgroundOfHSVSlider(ui->sldEndVal, value,ui->sldEndSat->value(),ui->sldEndVal->value());
    changeGradientStyleBasedOnSliders(ui);

    int layerIdx = ui->cmbGradient_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.eh, &l->eh, layerIdx, l, "eh:", true);
    ui->lblEndHuePercent->setText( QString::number(value));

}

void MainWindow::on_sldEndSat_valueChanged(int value)
{
    if (curSelection != ENDSAT){
       curSelection = ENDSAT;
       curHighlight = ENDSAT;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    changeBackgroundOfHSVSlider(ui->sldEndSat, ui->sldEndHue->value(),value,255);
    changeBackgroundOfHSVSlider(ui->sldEndVal, ui->sldEndHue->value(),value,ui->sldEndVal->value());
    changeGradientStyleBasedOnSliders(ui);

    int layerIdx = ui->cmbGradient_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.es, &l->es, layerIdx, l, "es:");
    ui->lblEndSatPercent->setText( QString::number(getPercentFromVal(value,255)) + "%");

}

void MainWindow::on_sldEndVal_valueChanged(int value)
{
    if (curSelection != ENDVAL){
       curSelection = ENDVAL;
       curHighlight = ENDVAL;
       highlightSlider(curHighlight);
       selectSlider(curSelection);
   }
    changeBackgroundOfHSVSlider(ui->sldEndVal, ui->sldEndHue->value(),ui->sldEndSat->value(),value);
    changeGradientStyleBasedOnSliders(ui);

    int layerIdx = ui->cmbGradient_Layers->currentData().toInt();
    Layer * l = nullptr;
    //if not master get the layer info
    if (layerIdx != -1){
        l = getLayerFromTable(layerIdx);
        //the arduino needs the overall layer idx not it's layer in the table
        layerIdx = getLayerIdx(l);
    }
    processSliderChange(value, &config.ev, &l->ev, layerIdx, l, "ev:");
    ui->lblEndValPercent->setText( QString::number(getPercentFromVal(value,255)) + "%");
}

void MainWindow::saveConfigToFile(){
    ofstream outfile(FILE_NAME.toStdString().c_str());
    if (!outfile.is_open()){
        displayStandardMessageBox("Error: could not save the settings to file!");
        cout << "Error could not open the settings file to save data!" <<endl;
        return;
    }

    //first line is rgb
    outfile << config.r << " " << config.g << " " << config.b << " " << config.d << " " << config.t << " " << config.w << " " << config.B << endl;
    //second is HSV
    outfile << config.h << " " << config.s << " " << config.v << endl;
    //third line is gradient
    outfile << config.sh << " " << config.ss << " " << config.sv << " " << config.eh << " " << config.es << " " << config.ev << endl;
    //last line is the mode
    outfile << static_cast<int>(config.tab) << endl;
    outfile << config.numLEDs << endl;
    //save master brightness
    outfile << config.MB << endl;
    //save strip type
    outfile << config.stripType << endl;
    //save the last open project to file
    outfile << config.lastOpenProject << endl;

    outfile.close();
}

void MainWindow::loadConfigFromFile(){
    ifstream infile(FILE_NAME.toStdString().c_str());
    if (!infile.is_open()){
        //TODO prompt to ask the user if they want to load a config file from somewhere?
        cout << "Created new settings file!" << endl;
        CONFIG newConf;
        newConf.r = 0; newConf.g = 0; newConf.b = 0; newConf.d = 0; newConf.t = 0; newConf.w = 0; newConf.B = 255;
        newConf.h = 0; newConf.s = 255; newConf.v = 255;
        newConf.sh = 0; newConf.ss = 255; newConf.sv = 255; newConf.eh = 0; newConf.es = 255; newConf.ev = 255;
        newConf.tab = 0;
        newConf.numLEDs = 10;//just show something, if it's 0 it seems like it's not working
        newConf.MB = 255;
        newConf.stripType = 0;
        newConf.lastOpenProject = "";
        config = newConf;
        saveConfigToFile();
        //also need to refresh the table because the combo groups are not drawn
        refreshProjectTable();
        //also load all sliders
        config.tab = TAB_HSV; loadSliders();
        config.tab = TAB_GRADIENT; loadSliders();
        config.tab = TAB_HOME;
    }

    //first line is rgb
    infile >> config.r >> config.g >> config.b >> config.d >> config.t >> config.w >> config.B;
    //second is HSV
    infile >> config.h >> config.s >> config.v;
    //third line is gradient
    infile >> config.sh >> config.ss >> config.sv >> config.eh >> config.es >> config.ev;
    //last line is the mode 
    infile >> config.tab ;
    infile >> config.numLEDs;
    infile >> config.MB;
    infile >> config.stripType;

    //read in the last open project
    infile.ignore(100,'\n');
    getline(infile,config.lastOpenProject);
    infile.close();

    cout << "---Loaded config settings---" << endl;
    cout << "NUM LEDs: " << config.numLEDs << endl;
    cout << "R,G,B,D,T,B:\t" << config.r << " "  << config.g  << " "  << config.b <<  " "  << config.d <<  " "  << config.t << " "  << config.B <<endl;
    cout << "H,S,V:\t" << config.h << " "  << config.s  << " "  << config.v << endl;
    cout << "GRADIENT" << endl;
    cout << "\tSTART--> H,S,V:\t" << config.sh << " "  << config.ss  << " "  << config.sv << endl;
    cout << "\tEND  --> H,S,V:\t" << config.eh << " "  << config.es  << " "  << config.ev << endl;
    cout << "MODE: " << config.tab << endl;
    cout << "Master Brightness: " << config.MB << endl;
    cout << "Strip Type: " << config.stripType << endl;
    cout << "Last Open Project " << config.lastOpenProject << endl;

    //set the num leds
    ui->edtNumLeds->blockSignals(true);
    ui->edtNumLeds->setPlainText( QString::number(config.numLEDs));
    ui->edtNumLeds->blockSignals(false);
    //sendArduinoCmd("nl:" + QString::number(config.numLEDs));

    ui->sldMasterRGB->blockSignals(true);
    ui->sldMasterRGB->setValue(config.MB);
    ui->sldMasterRGB->blockSignals(false);
    changeBackgroundOfRGBSlider(ui->sldMasterRGB, 244, 244, 220, config.MB/255.0);
    ui->lblMasterRGBPercent->setText( QString::number( getPercentFromVal(config.MB , 255)) + "%");

    ui->sldMasterHSV->blockSignals(true);
    ui->sldMasterHSV->setValue(config.MB);
    ui->sldMasterHSV->blockSignals(false);
    changeBackgroundOfRGBSlider(ui->sldMasterHSV, 244, 244, 220, config.MB/255.0);

    ui->sldMasterGradient->blockSignals(true);
    ui->sldMasterGradient->setValue(config.MB);
    ui->sldMasterGradient->blockSignals(false);
    changeBackgroundOfRGBSlider(ui->sldMasterGradient, 244, 244, 220, config.MB/255.0);

    //set the strip combo to match
    ui->cmbStripType->blockSignals(true);
    ui->cmbStripType->setCurrentIndex(config.stripType);
    ui->cmbStripType->blockSignals(false);

    changeWhiteSliderBasedOnCurStrip();

    //the arduino should have the same strip saved as the config file so dont resend
    //sendArduinoCmd("st:" + QString::number(config.stripType));
}

void MainWindow::loadProjectFromFile(string filename, bool firstLoad){

    //check if the folder exists
    if (!QDir(PROJECT_PATH).exists()){
        cout << "Creating the project directory" << endl;
        QDir().mkdir(PROJECT_PATH);
    }


    ifstream inFile(filename);
    if (!inFile.is_open()){
       displayStandardMessageBox(QString::fromStdString("Could not open project: " + filename));
       return;
    }

    //clear the arduino groups
    if (!firstLoad){
        sendArduinoCmd("g_rst");
        usleep(ARDUINO_MSG_DEL);
    }
    currentProject.clear();
    currentProject.readProjectFromFile(inFile);
    ui->edtProjectName->setPlainText(currentProject.name);
    refreshProjectTable();

    //you might load a file and not change anything so set it as the last open
    config.lastOpenProject = filename;
    saveConfigToFile();

}

void MainWindow::saveProjectToFile(){
    //replace the project name with underscores
    QString filename = PROJECT_PATH+ currentProject.name.replace(" ","_") + ".txt";

    //check if the folder exists
    if (!QDir(PROJECT_PATH).exists()){
        cout << "Creating the project directory" << endl;
        QDir().mkdir(PROJECT_PATH);
    }


    ofstream outfile(filename.toStdString());
    if (!outfile.is_open()){
        displayStandardMessageBox(QString::fromStdString("Could not save project to file: " + filename.toStdString()));
        return;
    }

    outfile << currentProject.toString();
    cout << "Updated " << currentProject.name.toStdString() << " file...\n";

    //since this was just saved also have that as the last one to load
    config.lastOpenProject = filename.toStdString();
    saveConfigToFile();
}

void MainWindow::on_tabWidget_tabBarClicked(int index)
{
    cout << "CLICKED: " << index << endl;
    config.tab = index;
    curHighlight = 0;
    curSelection = 0;

    //unhightlight whatever is highlighted
    /*for(int i =0; i < borders[config.tab].size(); i++){
        QString style = borders[config.tab][i]->styleSheet();
        //if the qsheet has black in it unselect
        if (style.contains(STYLE_HIGHLIGHT_BORDER) || style.contains(STYLE_SELECTED_BORDER)){
            borders[config.tab][i]->setStyleSheet(STYLE_WHITE_BORDER);
        }
    }*/

    highlightSlider(curHighlight);
    saveConfigToFile();

    //if Layers page do nothing
    if (index == TAB_HOME){
        return;
    }

    QString msg = "m:" + QString::number(index);
    //sendArduinoCmd(msg); HERE

    //load the master slider on the page that just got activated
    if (config.tab == TAB_RGB){
        ui->sldMasterRGB->blockSignals(true);
        ui->sldMasterRGB->setValue(config.MB);
        ui->sldMasterRGB->blockSignals(false);
    }
    else if (config.tab == TAB_HSV){
        ui->sldMasterHSV->blockSignals(true);
        ui->sldMasterHSV->setValue(config.MB);
        ui->sldMasterHSV->blockSignals(false);
    } else if (config.tab == TAB_GRADIENT){
        ui->sldMasterGradient->blockSignals(true);
        ui->sldMasterGradient->setValue(config.MB);
        ui->sldMasterGradient->blockSignals(false);
    }


    //if (!configDataSentToArduino[index]){
    sendTabData(config.tab);
    //}
}

void MainWindow::loadLayerToSliders(Layer * l){

    //Layer * g = getLayerFromName(name);

    if (l->type == MODE_RGB){
        changeBackgroundOfRGBSlider(ui->sldRed, 255,0,0, l->r / 255.0);
        ui->lblRedPercent->setText( QString::number( getPercentFromVal(l->r,255) ) );
        changeBackgroundOfRGBSlider(ui->sldGreen,0,255,0, l->g / 255.0);
        ui->lblGreenPercent->setText( QString::number( getPercentFromVal(l->g,255) ) );
        changeBackgroundOfRGBSlider(ui->sldBlue, 0,0,255, l->b / 255.0);
        ui->lblBluePercent->setText( QString::number( getPercentFromVal(l->b,255) ) );
        //changeBackgroundOfRGBSlider(ui->sldDaylight, 198, 243, 255, l->t / 255.0);
        //changeBackgroundOfRGBSlider(ui->sldTungsten, 255,212,125 , l->d / 255.0);
        changeBackgroundOfRGBSlider(ui->sldWhite, 255,255, 255 , l->w / 255.0);
        ui->lblWhitePercent->setText( QString::number( getPercentFromVal(l->w,255) ) );
        changeBackgroundOfRGBSlider(ui->sldBrightness,194,194,194, l->B / 255.0);
        ui->lblBrightnessPercent->setText( QString::number( getPercentFromVal(l->B,255) ) );

        setSliderSilent(ui->sldRed,l->r);
        setSliderSilent(ui->sldGreen,l->g);
        setSliderSilent(ui->sldBlue,l->b);
        //setSliderSilent(ui->sldDaylight,l->d);
        //setSliderSilent(ui->sldTungsten,l->t);
        setSliderSilent(ui->sldWhite,l->w);
        setSliderSilent(ui->sldBrightness,l->B);
    } else if (l->type == MODE_HSV){
        changeBackgroundOfHSVSlider(ui->sldHue, l->sh,180,255);
        changeBackgroundOfHSVSlider(ui->sldSat, l->sh,l->ss,255);
        changeBackgroundOfHSVSlider(ui->sldVal, l->sh,l->ss,l->sv);
        setSliderSilent(ui->sldHue,l->sh);
        setSliderSilent(ui->sldSat,l->ss);
        setSliderSilent(ui->sldVal,l->sv);
        ui->lblHuePercent->setText( QString::number( l->sh));
        ui->lblSatPercent->setText( QString::number( getPercentFromVal(l->ss,255) ) + "%");
        ui->lblValPercent->setText( QString::number( getPercentFromVal(l->sv,255) ) + "%");
    } else if (l->type == MODE_GRADIENT){
        //set start sliders
        changeBackgroundOfHSVSlider(ui->sldStartHue, l->sh,180,255);
        changeBackgroundOfHSVSlider(ui->sldStartSat, l->sh,l->ss,255);
        changeBackgroundOfHSVSlider(ui->sldStartVal, l->sh,l->ss,l->sv);
        setSliderSilent(ui->sldStartHue,l->sh);
        setSliderSilent(ui->sldStartSat,l->ss);
        setSliderSilent(ui->sldStartVal,l->sv);
        ui->lblStartHuePercent->setText( QString::number( l->sh));
        ui->lblStartSatPercent->setText( QString::number( getPercentFromVal(l->ss,255) ) + "%");
        ui->lblStartValPercent->setText( QString::number( getPercentFromVal(l->sv,255) ) + "%");
        //set end sliders
        changeBackgroundOfHSVSlider(ui->sldEndHue, l->eh,180,255);
        changeBackgroundOfHSVSlider(ui->sldEndSat, l->eh,l->es,255);
        changeBackgroundOfHSVSlider(ui->sldEndVal, l->eh,l->es,l->ev);
        setSliderSilent(ui->sldEndHue,l->eh);
        setSliderSilent(ui->sldEndSat,l->es);
        setSliderSilent(ui->sldEndVal,l->ev);
        ui->lblEndHuePercent->setText( QString::number( l->eh));
        ui->lblEndSatPercent->setText( QString::number( getPercentFromVal(l->es,255) ) + "%");
        ui->lblEndValPercent->setText( QString::number( getPercentFromVal(l->ev,255) ) + "%");

    }
}

void MainWindow::sendLayerInfo(Layer * l){

    //find the Layer in the list, assume all Layers are unique names
    int idx = getLayerIdx(l);


    QString msg = "gr{" + QString::number(idx);
    appendNum(msg, l->type);
    appendNum(msg, l->status);
    appendNum(msg, l->startLED);
    appendNum(msg, l->stopLED);
    if (l->type == MODE_RGB){
        appendNum(msg,l->r);
        appendNum(msg,l->g);
        appendNum(msg,l->b);
        appendNum(msg,l->d);
        appendNum(msg,l->t);
        appendNum(msg,l->w);
        appendNum(msg,l->B);
    } else if (l->type == MODE_HSV){
        appendNum(msg,l->sh);
        appendNum(msg,l->ss);
        appendNum(msg,l->sv);
    } else if (l->type == MODE_GRADIENT){
        appendNum(msg,l->sh);
        appendNum(msg,l->ss);
        appendNum(msg,l->sv);
        appendNum(msg,l->eh);
        appendNum(msg,l->es);
        appendNum(msg,l->ev);
    }

    msg += "}";
    sendArduinoCmd(msg);
   // cout << "Sent: " << msg.toStdString() << endl;
}

void MainWindow::setSliderSilent(QSlider *qs, int val){
    qs->blockSignals(true);
    qs->setValue(val);
    qs->blockSignals(false);
}

void MainWindow::displayStandardMessageBox(QString msg){
    QMessageBox msgBox(this);
    msgBox.setText(msg);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

//see if this layers LEDs overlap with another layer in the group
bool MainWindow::isValidLayerLED(int led, Layer * layerOfLEDChange){

    if (led < 0 || led >= config.numLEDs){
        QMessageBox msgBox;
        msgBox.setText("Please choose an LED from 0 to NUM LEDS ( 0 to " + QString::number(config.numLEDs) + " )");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return false;
    }



    //go through all Layers, if the led passed in lives within another range return false
    for(size_t g = 0; g < currentProject.groups.size(); g++){
        Group * curGroup  = &currentProject.groups[g];
        for(size_t i =0; i < curGroup->layers.size(); i++){
            //we will always be in the range of our own Layer
            if (&curGroup->layers[i] != layerOfLEDChange && curGroup->layers[i].startLED <= led && led <= curGroup->layers[i].stopLED){
                return false;
            }
        }
    }
    return true;
}

Group * MainWindow::getGroupLayerBelongsToBasedOnRow(int row){
    int rowCo = 0;
    //based on the row in the table find the group that this LED lives in
    for(size_t i = 0; i < currentProject.groups.size(); i++){
        rowCo++; //count this group
        //add the layers for this group
        rowCo += currentProject.groups[i].layers.size();
        if (row < rowCo){
            return &currentProject.groups[i];
            break;
        }
    }
    return nullptr;
}

void MainWindow::on_tblGroups_cellChanged(int row, int column)
{
    //---SPLIT THIS INTO EDITING A GROUP AND EDITING A LAYER---
    int cellType = getRowObjectFromTable(row);
    QTableWidgetItem * cell = ui->tblGroups->item(row,column);
    int madeChange = 0;
    //---EDIT GROUP---
    if (cellType == CELL_TYPE_GROUP){
        Group * groupToChange = getGroupFromTable(row);

        if (column == COL_GROUP_NAME){
            //check to see if the group name exists
            int exists = 0;
            for(size_t i = 0; i < currentProject.groups.size(); i++){
                if (currentProject.groups[i].name.toLower() == cell->text().toLower()){
                    displayStandardMessageBox("You cannot have two Groups in the same project with the same name. Please pick another name");
                    exists = 1;
                    break;
                }
            }
            if (!exists){
                groupToChange->name = cell->text();
                madeChange =1;
            } else {
                //put the name back to what it was
                ui->tblGroups->blockSignals(true);
                cell->setText(groupToChange->name);
                ui->tblGroups->blockSignals(false);
            }

        }

        if (madeChange == 1){
            saveProjectToFile();
        }

    } else if (cellType == CELL_TYPE_LAYER){
        Layer * layerToChange = getLayerFromTable(row);
        //find the group that this layer belongs to
        Group * groupLayerBelongsTo = getGroupLayerBelongsToBasedOnRow(row);



        if (column ==  COL_LAYER_NAME){
                   //we need unique names for everthing so check if this exists already
                int exists = 0;
                for(size_t i = 0; i < groupLayerBelongsTo->layers.size(); i++){
                    if (groupLayerBelongsTo->layers[i].name.toLower() == cell->text().toLower()){
                        displayStandardMessageBox("You cannot have two Layers in the same group with the same name. Please pick another name");
                        exists = 1;
                        break;
                    }
                }
                if (!exists){
                    layerToChange->name = cell->text();
                    madeChange =1;
                } else {
                    //put the name back to what it was
                    ui->tblGroups->blockSignals(true);
                    cell->setText(layerToChange->name);
                    ui->tblGroups->blockSignals(false);
                }
        } else if (column == COL_STARTLED){

            //make sure they put a number in cell
            bool isNumber;
            int startLED = cell->text().toInt(&isNumber);
            if (!isNumber){
                displayStandardMessageBox("Please only use numeric values for the start led.");
                //change cell back to original value
                ui->tblGroups->blockSignals(true);
                cell->setText(QString::number(layerToChange->startLED));
                ui->tblGroups->blockSignals(false);
                return;
            }

            //pass in the row that you're tring to change
                if (isValidLayerLED(startLED, layerToChange) ){
                    layerToChange->startLED = startLED;
                    madeChange =1;
                } else {
                    displayStandardMessageBox("The LED overlaps with another layer in the project! Choose another LED");
                    //set the cell text back to what it was
                    //but dont fire the cell changed again
                    ui->tblGroups->blockSignals(true);
                    cell->setText(QString::number(layerToChange->startLED));
                    ui->tblGroups->blockSignals(false);
                }
        } else if(column == COL_STOPLED){

            //make sure they put a number in cell
            bool isNumber;
            int stopLED = cell->text().toInt(&isNumber);
            if (!isNumber){
                displayStandardMessageBox("Please only use numeric values for the stop led.");
                //change cell back to original value
                ui->tblGroups->blockSignals(true);
                cell->setText(QString::number(layerToChange->stopLED));
                ui->tblGroups->blockSignals(false);
                return;
            }

                 if (isValidLayerLED(stopLED, layerToChange)){
                    madeChange =1;
                    layerToChange->stopLED = stopLED;
                 }else {
                     displayStandardMessageBox("The LED overlaps with another Layer! Choose another LED");
                     //set the cell text back to what it was
                     ui->tblGroups->blockSignals(true);
                     cell->setText(QString::number(layerToChange->stopLED));
                     ui->tblGroups->blockSignals(false);
                 }
        }

        if (madeChange == 1){
            saveProjectToFile();
            sendLayerInfo(layerToChange);
        }
    }





}

void MainWindow::on_cmbRGB_Layers_activated(int index)
{

    /*QVariant itemData = ui->cmbRGB_Layers->currentData();
    int idx = ui->cmbRGB_Layers->currentIndex();
    cout << "Index in COMBO: " << idx << " maps to row " << itemData.toInt() << " in table" << endl;
    return;*/

    //if it's on master then change the whole entire strip
    if (index == 0){
        loadSliders();
        //also it's possible that the Layer is on HSV mode and you hit the edit button from the Layers page
        //so we may have changed to master but the LED strip isn't in RGB mode so check that
        if (config.tab != TAB_RGB){
            config.tab = TAB_RGB;
            sendTabData(TAB_RGB);
        }
    } else {
        int layerIdxTable = ui->cmbRGB_Layers->currentData().toInt();
        loadLayerToSliders( getLayerFromTable(layerIdxTable));
    }
}

void MainWindow::on_cmbHSV_Layers_activated(int index)
{
    //if it's on master then change the whole entire strip
    if (index == 0){
        loadSliders();
        //check if the overall mode matches the tab we're on
        if (config.tab != TAB_HSV){
            config.tab = TAB_HSV;
            sendTabData(TAB_HSV);
        }
    } else {
        int layerIdxTable = ui->cmbHSV_Layers->currentData().toInt();
        loadLayerToSliders( getLayerFromTable(layerIdxTable));
    }
}

void MainWindow::on_cmbGradient_Layers_activated(int index)
{
    //if it's on master then change the whole entire strip
    if (index == 0){
        loadSliders();
        //check if the overall mode matches the tab we're on
        if (config.tab != TAB_GRADIENT){
            config.tab = TAB_GRADIENT;
            sendTabData(TAB_GRADIENT);
        }
    } else {
        int layerIdxTable = ui->cmbGradient_Layers->currentData().toInt();
        loadLayerToSliders( getLayerFromTable(layerIdxTable));
    }
}

void MainWindow::on_edtNumLeds_textChanged()
{
    QString numLedsStr = ui->edtNumLeds->toPlainText();
    if (numLedsStr.length() == 0){
        return;
    }
    bool ok;
    int numLeds = numLedsStr.toInt(&ok);
    if (ok == false){
        QMessageBox msgBox;
        msgBox.setText("Please enter a number!");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }

    sendArduinoCmd("nl:" + QString::number(numLeds));
    config.numLEDs = numLeds;
    saveConfigToFile();

}

void MainWindow::on_sldMasterBrightness(int val){
    //qDebug() << "Master: " << val;

    if ( abs(config.MB - val) > SLD_CHANGE_THRESHOLD || val > 252 )
    {
        config.MB = val;
        saveConfigToFile();
        sendArduinoCmd("MB:" + QString::number(val));

        double percent = (double) val / 255;
        int p = percent * 100;
        //244,244,220
        if (config.tab == MODE_RGB){
            changeBackgroundOfRGBSlider(ui->sldMasterRGB, 244, 244, 220, config.MB/255.0);
            ui->lblMasterRGBPercent->setText( QString::number(p) + "%");
        } else if (config.tab == MODE_HSV){
            changeBackgroundOfRGBSlider(ui->sldMasterHSV, 244, 244, 220, config.MB/255.0);
        } else if (config.tab == MODE_GRADIENT){
            changeBackgroundOfRGBSlider(ui->sldMasterGradient, 244, 244, 220, config.MB/255.0);
        }

    }

}

void MainWindow::changeWhiteSliderBasedOnCurStrip(){

    //dim the white with it's not compatible
    if (config.stripType == STRIP_WS2812B || config.stripType == STRIP_APA102){
        ui->lblWhite->setStyleSheet("color: gray");
        ui->lblWhitePercent->setStyleSheet("color: gray");
        ui->lblWhitePercent->setVisible(false);
        ui->sldWhite->setEnabled(false);
    } else {
        ui->lblWhite->setStyleSheet("color: black");
        ui->lblWhitePercent->setStyleSheet("color: black");
        ui->lblWhitePercent->setVisible(true);
        ui->sldWhite->setEnabled(true);
    }
}

//this should only fire when the user actually interacts with it
//not an index being set
void MainWindow::on_cmbStripType_activated(int index)
{
    config.stripType = index;

    changeWhiteSliderBasedOnCurStrip();

    saveConfigToFile();
    //send the change to arduino and reset it
    sendArduinoCmd("st:" + QString::number(index));
    //the reset happens from the st command in arduino
    //sendArduinoCmd("RESET");
    usleep(1000*1000); //wait 1 second for the reset

    //close serial communication
    arduino->close();
    //wait for it to connect again
    while (!arduino->isOpen()){
        arduino->reconnect();
    }
    usleep(2000*1000); //need this two second delay on startup to connected

    //after it resets see if it will reconnect
    //then resend the initial data
    sendStartUpData(false);
}

void MainWindow::drawLayer(Layer cur, int nextRow){

        //make a row in the table
        ui->tblGroups->insertRow(nextRow);
        //make an item with a white background so it has to grid lines
        QTableWidgetItem * item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled);
        item->setBackground(QBrush(QColor(255, 255, 255)));

        ui->tblGroups->setItem(nextRow,COL_GROUP_NAME, new QTableWidgetItem(""));
        ui->tblGroups->setItem(nextRow,COL_LAYER_NAME, new QTableWidgetItem(cur.name));
        ui->tblGroups->setItem(nextRow,COL_STARTLED, new QTableWidgetItem( QString::number(cur.startLED)));
        ui->tblGroups->setItem(nextRow,COL_STOPLED, new QTableWidgetItem(QString::number(cur.stopLED)));
        QComboBox * cmbMode = createLayerModeCombo(nextRow, cur.type);
        ui->tblGroups->setCellWidget(nextRow,COL_MODE, cmbMode);
        QPushButton * btnEdit = createLayerEditButton(nextRow);
        ui->tblGroups->setCellWidget(nextRow,COL_EDITBTN, btnEdit);

        QComboBox * cmbStatus = createLayerStatusCombo(nextRow, cur.status);
        ui->tblGroups->setCellWidget(nextRow,COL_STATUS, cmbStatus);

        //if this Layer is active put it on the correct list
        if (cur.status == ACTIVE){
            Group * g = getGroupLayerBelongsToBasedOnRow(nextRow);
            //add the layer to the correct combo box
            if (cur.type == MODE_RGB){              
               ui->cmbRGB_Layers->addItem(g->name + ": " + cur.name, QVariant(nextRow));
            } else if (cur.type == MODE_HSV){
                ui->cmbHSV_Layers->addItem(g->name + ": " + cur.name, QVariant(nextRow));
             } else if (cur.type == MODE_GRADIENT){
                ui->cmbGradient_Layers->addItem(g->name + ": " + cur.name, QVariant(nextRow));
             }
        }



}

void MainWindow::drawGroup(Group cur, int nextRow){

    ui->tblGroups->insertRow(nextRow);
    ui->tblGroups->setItem(nextRow,COL_GROUP_NAME, new QTableWidgetItem(cur.name));
    ui->tblGroups->setItem(nextRow,COL_STARTLED, new QTableWidgetItem( ""));
    ui->tblGroups->setItem(nextRow,COL_STOPLED, new QTableWidgetItem(""));
    ui->tblGroups->setItem(nextRow,COL_MODE, new QTableWidgetItem(""));
    ui->tblGroups->setItem(nextRow,COL_EDITBTN, new QTableWidgetItem(""));
    //this combo will change the sub layers of the group but it happens inside this function
    QComboBox * cmbStatus = createLayerStatusCombo(nextRow, cur.status);
    ui->tblGroups->setCellWidget(nextRow,COL_STATUS, cmbStatus);

    ui->tblGroups->setItem(nextRow,COL_STATUS, new QTableWidgetItem(""));


}

void MainWindow::refreshProjectTable(){


    cout << "Refreshing table with " << currentProject.name.toStdString() << "...\n";

    //clear the current list
    ui->tblGroups->model()->removeRows(0, ui2->tblGroups->rowCount());

    //Add the MASTER option
    ui->cmbRGB_Layers->clear();
    ui->cmbRGB_Layers->addItem("Master", QVariant(-1));
    ui->cmbHSV_Layers->clear();
    ui->cmbHSV_Layers->addItem("Master", QVariant(-1));
    ui->cmbGradient_Layers->clear();
    ui->cmbGradient_Layers->addItem("Master", QVariant(-1));


    int rowCo = 0;
    ui->tblGroups->blockSignals(true);
    //go through all groups
    for(size_t g = 0; g < currentProject.groups.size(); g++){
        drawGroup(currentProject.groups[g], rowCo++);
        //go through all layers
        vector<Layer> groupLayers = currentProject.groups[g].layers;
        for(size_t l = 0; l < groupLayers.size(); l++){
            //drawing the layers adds them to the correct combo boxes
           drawLayer(groupLayers[l],rowCo++);
        }
    }

    ui->tblGroups->blockSignals(false);

    //TODO right now the selection goes away so clear the copy and paste, delete group and layer
    ui->btnCopyTo->setEnabled(false);
    ui->btnPaste->setEnabled(false);
    ui->btnDeleteGroup->setEnabled(false);
    ui->btnDeleteLayer->setEnabled(false);


}

void MainWindow::on_btnNewProject_clicked()
{
    ui->edtProjectName->setPlainText("");
    currentProject.clear();
    //clear the table
    refreshProjectTable();
    sendArduinoCmd("g_rst");

}

void MainWindow::on_btnAddGroup_clicked()
{

    //grab the name from the edt box
    currentProject.name = ui->edtProjectName->toPlainText();

    if (currentProject.name == ""){
        displayStandardMessageBox("You have not named this project yet. Please name the project before adding groups.");
        return;
    }
    //check that something is selected

    Group g;
    g.name = "Group " + QString::number(currentProject.groups.size() + 1);
    g.status = ACTIVE;
    currentProject.addGroup(g);
    refreshProjectTable();
    saveProjectToFile();
}

void MainWindow::on_btnAddLayer_clicked()
{

    //TODO - the selection of the table should stay after you hit add layer
    //right now a group is highlighted, add layer adds a new layer and refreshes the table -the selection needs to come back afterwards

    int idxGroup = checkTableSelection("Please click on the Group name you would like to add the new layer to");
    if (idxGroup == -1){
        return;
    }

    //get the group to add the layer to
    Group * g = getGroupFromTable(idxGroup);
    //TODO bug - when hit the EDIT on a layer, it counts as a selection but then goes to another screen
    //when you come back to the home page the old selection is still there, should really clear out the
    //selection model when you come back
    //This works for now, if you have a layer highlighted the group comes back null
    if (g == nullptr){
        displayStandardMessageBox("Please click on the Group name you would like to add the new layer to");
        return;
    }

    Layer l = {"",0,0,0,   0,0,0,0,0,0,255,  0,255,255,0,255,255,  0,0,0};
    l.name = "Layer " + QString::number(g->layers.size() + 1);
    g->addLayer(l);
    refreshProjectTable();
    saveProjectToFile();
    //TODO if the layer you're adding is in the last group of the table then you dont need to resend everything
    //just the last layer
    Group * lastGroup = &currentProject.groups[ currentProject.groups.size()-1];
    if (g == lastGroup){
        sendLayerInfo(&g->layers[g->layers.size() - 1]);
    } else {
        sendAllLayerInfo();
    }
}

void MainWindow::on_btnDeleteLayer_clicked()
{


    int idxToDelete = checkTableSelection("Please click on the Layer name you want to delete first.");
    if (idxToDelete == -1){
        return;
    }

    Layer * l = getLayerFromTable(idxToDelete);
    QMessageBox msgBox;
    msgBox.setText("Are you sure you want to delete " + l->name);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    int ret = msgBox.exec();

   if (ret == QMessageBox::Yes){
        cout << "Deleting index " << idxToDelete << " from project" << endl;
        deleteItemFromProject(nullptr, l);
        refreshProjectTable();
        //TODO right now quicker to just resend all the group data but maybe with 100 layers that's a bad move?
        saveProjectToFile();
        sendAllLayerInfo();
    }
}

void MainWindow::on_btnDeleteGroup_clicked()
{

    int idxToDelete = checkTableSelection("Please click on the Group name you want to delete first.");
    if (idxToDelete == -1){
        return;
    }

    int cellType = getRowObjectFromTable(idxToDelete);

    if (cellType != CELL_TYPE_GROUP){
        displayStandardMessageBox("A group is not selected. Please select a group, or try to delete a layer.");
        return;
    }

    Group * g = getGroupFromTable(idxToDelete);
    QMessageBox msgBox;
    msgBox.setText("Are you sure you want to delete " + g->name);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    int ret = msgBox.exec();

   if (ret == QMessageBox::Yes){
        cout << "Deleting group at idx " << idxToDelete << " from table!" << endl;
        //delete all layers first, actually no deleting the entire group should get rid of them
        /*for(size_t i =0; i < g->layers.size(); i++){
            deleteItemFromProject(nullptr, &g->layers[i]);
        }*/
        deleteItemFromProject(g,nullptr);
        refreshProjectTable();
        saveProjectToFile();
        sendArduinoCmd("g_rst");
        sendAllLayerInfo();
    }
}

void MainWindow::deleteItemFromProject(Group * group, Layer * layer){


    for(size_t g = 0; g < currentProject.groups.size(); g++){
        //if group pointer was sent in and we found it, delete it
        if (group != nullptr && group == &currentProject.groups[g]){
            currentProject.groups.erase(currentProject.groups.begin() + g);
            currentProject.numGroups--;
            return;
        }

        //only check layers if you need to
        if (layer != nullptr){
            for(size_t l = 0; l < currentProject.groups[g].layers.size(); l++){
                //if layer pointer was sent in and we found it delete it
                if (layer == &currentProject.groups[g].layers[l]){
                    currentProject.groups[g].layers.erase(currentProject.groups[g].layers.begin() + l);
                    currentProject.groups[g].numLayers--;
                    return;
                }

            }
        }
    }
}

Group * MainWindow::getGroupFromTable(int row){

    int rowCo = 0;
    for(size_t g = 0; g < currentProject.groups.size(); g++){
        if (rowCo == row){
            return &currentProject.groups[g];
        }
        rowCo++;
        //add a row for every layer
        rowCo += currentProject.groups[g].layers.size();
    }
    return nullptr;
}

Layer * MainWindow::getLayerFromTable(int row){
    int rowCo = 0;
    for(size_t g = 0; g < currentProject.groups.size(); g++){
       //inc row for every group
        rowCo++;
        for(size_t l = 0; l < currentProject.groups[g].layers.size(); l++){
            if (rowCo == row){
                return &currentProject.groups[g].layers[l];
            }
            rowCo++;
        }
    }
    //should never happen but make compiler happy
    return nullptr;
}

//pointers default to null if not supplied
int MainWindow::getRowObjectFromTable(int idx){

    vector<Group> groups = currentProject.groups;
    int rowCo = 0;
    for(size_t g = 0; g < groups.size(); g++){
        if (rowCo == idx){
            return CELL_TYPE_GROUP;
        }
        rowCo++;
        for(size_t l = 0; l < groups[g].layers.size(); l++){
            if (rowCo == idx){
                return CELL_TYPE_LAYER;
            }
            rowCo++;
        }
    }

    //should always be found but make compiler happy
    return -1;

}

void MainWindow::on_tblGroups_cellClicked(int row, int column)
{
    int celltype = getRowObjectFromTable(row);
    if (celltype == CELL_TYPE_LAYER){
        //clicked layer
        ui->btnDeleteLayer->setEnabled(true);
        ui->btnDeleteGroup->setEnabled(false);
        ui->btnCopyTo->setEnabled(true);
    } else if (celltype == CELL_TYPE_GROUP){
        ui->btnDeleteLayer->setEnabled(false);
        ui->btnDeleteGroup->setEnabled(true);
        ui->btnCopyTo->setEnabled(true);
    }
}

int MainWindow::checkTableSelection(QString errorMsg){
    QModelIndexList selection = ui->tblGroups->selectionModel()->selectedIndexes();
    //TODO should this delete multiple selections?
    if (selection.size() == 0 || selection.size() > 1){
        QMessageBox msgBox;
        msgBox.setText(errorMsg);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return -1;
    }
    return selection.at(0).row();
}

void MainWindow::on_btnSaveProject_clicked()
{
    saveProjectToFile();
}

void MainWindow::on_btnCopyProject_clicked()
{
    displayStandardMessageBox("This does not work yet. It will copy the current project to a new project with a different name");
}

void MainWindow::on_edtProjectName_textChanged()
{
    currentProject.name = ui->edtProjectName->toPlainText();
}

void MainWindow::on_btnLoadProject_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,"Choose A Project",PROJECT_PATH);
    cout << filename.toStdString() << endl;
    loadProjectFromFile(filename.toStdString());
    sendAllLayerInfo();

}

void MainWindow::on_btnCopyTo_clicked()
{
    int idxSelect = checkTableSelection("Please click on a Group or Layer to copy it");
    if (idxSelect == -1){
        return;
    }
    int cellType = getRowObjectFromTable(idxSelect);
    if (cellType == CELL_TYPE_GROUP){
        Group * g = getGroupFromTable(idxSelect);
        //copy this to a new group
        Group newGroup;
        newGroup.copyGroup(g);
        currentProject.addGroup(newGroup);
        refreshProjectTable();
        sendAllLayerInfo();
        //TODO save this project
        //saveProjectToFile();
    } else if (cellType == CELL_TYPE_LAYER){
        layerOnClipboard = getLayerFromTable(idxSelect);
        //paste becomes active, then click the group to paste it on and it will
        ui->btnPaste->setEnabled(true);
    }

}

void MainWindow::on_btnPaste_clicked()
{
    int idxSelect = checkTableSelection("Please click on a Group to paste the last copied layer.");
    if (idxSelect == -1){
        return;
    }
    int cellType = getRowObjectFromTable(idxSelect);
    if (cellType == CELL_TYPE_LAYER){
        displayStandardMessageBox("Please click on a Group to paste the last copied layer.");
    } else if (cellType == CELL_TYPE_GROUP){
        Group * g = getGroupFromTable(idxSelect);
        Layer l;
        l.copyLayer(layerOnClipboard);
        g->addLayer(l);
        refreshProjectTable();
        sendAllLayerInfo();
        //TODO save this project
        //saveProjectToFile();

        //after a paste turn off the button
        ui->btnPaste->setEnabled(false);
    }
}

