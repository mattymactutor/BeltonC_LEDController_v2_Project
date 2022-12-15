#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QThread>
#include <QCheckBox>
#include <QStringListModel>
#include <string>
#include "Serial_Comm_Footpedal.h"
#include <sstream>
#include <fstream>

using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE



#define ACTIVE 0
#define INACTIVE 1
#define OFF 2
#define BLINK 3
struct Layer
{
public:
  QString name;
  int startLED;
  int stopLED;
  int type; //RGB, HSV, GRADIENT
  int r, g, b, d, t, w, B;
  int sh, ss, sv, eh, es, ev;
  int status; //ACTIVE,INACTIVE,OFF,BLINK
  int onTime;
  int offTime;

  void copyLayer(Layer * l){
      name = l->name;
      startLED = 0;
      stopLED = 0;
      type = l->type;
      r = l->r;
      g = l->g;
      b= l->b;
      d= l->d;
      t= l->t; w=l->w; B=l->B;
      sh=l->sh; ss=l->ss; sv=l->sv; eh=l->eh; es=l->es; ev=l->ev;
      status = l->status;
      onTime = l->onTime;
      offTime = l->offTime;
  }

  void fromLine(string line){
      istringstream s(line);
      string temp;
      getline(s,temp,','); name = QString::fromStdString(temp);
      //use getline so you can read up to the comma
      getline(s,temp,','); istringstream(temp) >> startLED;
      getline(s,temp,','); istringstream(temp) >> stopLED;
      getline(s,temp,','); istringstream(temp) >> type;
      getline(s,temp,','); istringstream(temp) >> r;
      getline(s,temp,','); istringstream(temp) >> g;
      getline(s,temp,','); istringstream(temp) >> b;
      getline(s,temp,','); istringstream(temp) >> d;
      getline(s,temp,','); istringstream(temp) >> t;
      getline(s,temp,','); istringstream(temp) >> w;
      getline(s,temp,','); istringstream(temp) >> B;
      getline(s,temp,','); istringstream(temp) >> sh;
      getline(s,temp,','); istringstream(temp) >> ss;
      getline(s,temp,','); istringstream(temp) >> sv;
      getline(s,temp,','); istringstream(temp) >> eh;
      getline(s,temp,','); istringstream(temp) >> es;
      getline(s,temp,','); istringstream(temp) >> ev;
      getline(s,temp,','); istringstream(temp) >> status;
      getline(s,temp,','); istringstream(temp) >> onTime;
      getline(s,temp,','); istringstream(temp) >> offTime;

  }

  string toString(){
      stringstream output;
      output << name.toStdString() << "," << startLED << "," << stopLED << "," << type << ",";
      output << r << ","<< g<< "," << b << ","  << d << "," << t << "," << w << "," << B << ",";
      output << sh << ","<< ss<< "," << sv << ","  << eh << "," << es << "," << ev << "," << status << "," << onTime <<","<<offTime;
      return output.str();
  }
};

struct Group{
    QString name;
    vector<Layer> layers;
    int numLayers = 0;
    int status = 0;

    void copyGroup(Group * g){
        name = g->name;
        status = g->status;
        for(size_t i = 0; i < g->layers.size(); i++){
            Layer l = g->layers[i];
            l.startLED = 0;
            l.stopLED = 0;
            addLayer(l);
        }
    }


    void addLayer(Layer l){
        layers.push_back(l);
        numLayers = layers.size();
    }
    void readGroupFromFile(ifstream &inFile){
        //-Group 1, 3, 0
        //--Layer1,1,2,3,
        //--Layer2,1,2,3
        //--Layer3,1,2,3

        //read in the first -
        char hyphen;
        inFile.get(hyphen);

        string temp;
        getline(inFile, temp, ','); name = QString::fromStdString(temp);
        int layersInFile;
        getline(inFile, temp,','); istringstream(temp) >> layersInFile;
        getline(inFile, temp); istringstream(temp) >> status;
        for(int i = 0; i < layersInFile; i++){
            getline(inFile,temp);
            //take off the 2 hyphens
            temp = temp.substr(2);
            Layer l;
            l.fromLine(temp);
            addLayer(l);
        }

    }

    string toString(){
        stringstream output;
        output << name.toStdString() << "," << numLayers << "," << status << endl;
        //group is one - nested
        //groups are double -- nested

        for(int i = 0; i < numLayers; i++){
            output << "--" << layers[i].toString();
            if (i < numLayers - 1){
                output << endl;
            }
        }

        return output.str();
    }
};

struct Project{
    QString name;
    vector<Group> groups;
    int numGroups;

    void clear(){
        groups.clear();
        numGroups = 0;
        name = "";
    }

    int getTotalLayers(){
        int sum = 0;
          for(size_t i = 0; i < groups.size(); i++){
            sum = groups[i].numLayers;
        }
        return sum;
    }

    void addGroup(Group g){
        groups.push_back(g);
        numGroups = groups.size();
    }

    void readProjectFromFile(ifstream &inFile){
        //ProjectName,1
        //-Group 1,3
        //--Layer1,1,2,3,
        //--Layer2,1,2,3
        //--Layer3,1,2,3


        string temp;
        getline(inFile, temp, ','); name = QString::fromStdString(temp);
        if (name == ""){
            cout << "Empty File" << endl;
            return;
        }

        int groupsInFile;
        getline(inFile, temp); istringstream(temp) >> groupsInFile;
        for(int i = 0; i < groupsInFile; i++){
            //the group structure takes the hyphens off
            Group g;
            g.readGroupFromFile(inFile);
            addGroup(g);
        }

    }

    string toString(){
        stringstream output;
        output << name.toStdString() << "," << numGroups << endl;

        for(int i = 0; i < numGroups; i++){
            output << "-" << groups[i].toString() << endl;
        }

        return output.str();

    }
};

//make a strucutre to keep track of the settings
struct CONFIG{
    int h, s, v;
    //rgb daylight tungsten brightness
    int r, g, b, d, t, w, B;
    int sh,ss,sv,eh,es,ev;
    int tab;
    int numLEDs;
    int MB;
    int stripType;
    string lastOpenProject = "";
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //int curHighlight = 0, curSelection = -1;
    //void highlightSlider(int sld);
    //void selectSlider(int sld);

private:
    Ui::MainWindow *ui;    
    double lastArduinoSend = 0;
    void loadConfigFromFile();
    void saveConfigToFile();    
    //this function resets the arduino data so only do that if not first load
    void loadProjectFromFile(string filename, bool firstLoad = false); //filename comes from a pop up
    void saveProjectToFile(); //current project
    void refreshProjectTable();
    void drawLayer(Layer l, int nextRow);
    void drawGroup(Group g, int nextRow);
    void sendTabData(int idx);
    void sendStartUpData(bool firstLoad);
    void sendArduinoCmd(QString in);
    void sendLayerInfo(Layer * l);
    void loadSliders();
    void loadLayerToSliders(Layer *);
    void setSliderSilent(QSlider * qs, int val);
    void sendAllLayerInfo();
    void changeWhiteSliderBasedOnCurStrip();
    void displayStandardMessageBox(QString msg);
    //this function will return the object at the index, if it is a layer, g is null and otherwise
    int getRowObjectFromTable(int idx);
    Group * getGroupFromTable(int row);
    Group * getGroupLayerBelongsToBasedOnRow(int row);
    Layer * getLayerFromTable(int row);
    void deleteItemFromProject(Group * g, Layer * l);
    int checkTableSelection(QString errorMsg);
    int getLayerIdx(Layer * l);
    void processSliderChange(int value, int * configVal, int * LayerVal, int layerIdx,  Layer *l, QString arduinoCmd,bool hsvConvert = false);
    QComboBox * createLayerModeCombo(int row, int idx);
    QComboBox * createLayerStatusCombo(int row, int idx);
    QPushButton * createLayerEditButton(int row);
    bool isValidLayerLED(int led, Layer * l);

   // void parseUSBCmd(string in);


private slots:
       void on_sldMasterBrightness(int idx);
       void tableComboModeChanged(int idx);
       void tableComboStatusChanged(int idx);
       void on_btnClose_clicked();       
       void on_sldRed_valueChanged(int value);
       void on_sldGreen_valueChanged(int value);
       void on_sldBlue_valueChanged(int value);
       //void on_sldTungsten_valueChanged(int value);
       //void on_sldDaylight_valueChanged(int value);
       void on_sldBrightness_valueChanged(int value);
       void on_sldHue_valueChanged(int value);
       void on_sldSat_valueChanged(int value);
       void on_sldVal_valueChanged(int value);
       void on_sldStartHue_valueChanged(int value);
       void on_sldStartSat_valueChanged(int value);
       void on_sldStartVal_valueChanged(int value);
       void on_sldEndHue_valueChanged(int value);
       void on_sldEndSat_valueChanged(int value);
       void on_sldEndVal_valueChanged(int value);
       void on_tabWidget_tabBarClicked(int index);
       void on_btnAddLayer_clicked();
       void on_tblGroups_cellChanged(int row, int column);
       void on_edtNumLeds_textChanged();
       void on_btnDeleteLayer_clicked();
       void on_cmbStripType_activated(int index);
       void on_sldWhite_valueChanged(int value);
       void on_cmbRGB_Layers_activated(int index);
       void on_cmbHSV_Layers_activated(int index);
       void on_cmbGradient_Layers_activated(int index);
       void on_btnNewProject_clicked();
       void on_btnAddGroup_clicked();
       void on_btnDeleteGroup_clicked();
       void on_tblGroups_cellClicked(int row, int column);
       void on_btnSaveProject_clicked();
       void on_btnCopyProject_clicked();
       void on_edtProjectName_textChanged();
       void on_btnLoadProject_clicked();
       void on_btnCopyTo_clicked();
       void on_btnPaste_clicked();
};
#endif // MAINWINDOW_H
