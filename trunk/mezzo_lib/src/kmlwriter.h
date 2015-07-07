#ifndef KML_WRITER_HH
#define KML_WRITER_HH
#include <QFile>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QXmlStreamWriter>
#include <iostream>

class KmlWriter
{
public:
	KmlWriter(const QString & filename_):filename(filename_) {}
	void init();
	void close(); 
	void add_style(QString id, QString color, QString width);
	void start_folder(QString name);
	void end_folder() {writer.writeEndElement();}
	void write_link_placemark(QString name, QString begin, QString end, QString style, QString coordinates);
private:
	QString filename;
	QFile file;
	QXmlStreamWriter writer;


};

#endif //KML_WRITER_HH














/*



int main ( int argc, char **argv)
{
	QString m_filename = "output.kml";
	QFile file(m_filename);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cout << "Unable to write to " << m_filename.toStdString();
        return -1;
    }

    QXmlStreamWriter writer;
	//Initialise
    writer.setDevice(&file);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(4);
    writer.writeStartDocument();
    writer.writeStartElement("kml");
    writer.writeAttribute("xmlns", "http://www.opengis.net/kml/2.2");
	writer.writeAttribute("xmlns:gx" , "http://www.google.com/kml/ext/2.2" );
	writer.writeAttribute("xmlns:atom" , "http://www.w3.org/2005/Atom" );
    //writer.writeAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    writer.writeStartElement("Document");
	writer.writeAttribute ("id", "");
	writer.writeTextElement("name","output.kml");
	writer.writeTextElement("open","1");
	
	//Style
	writer.writeStartElement("Style");
	writer.writeAttribute ("id", "style0");
	writer.writeStartElement("LineStyle");
	writer.writeTextElement ("color", "FF00FF00");
	writer.writeTextElement ("width", "4.0");
	writer.writeEndElement(); //LneStyle
	writer.writeEndElement(); // Style	

	//Link placemarks
	writer.writeStartElement("Folder");
	writer.writeTextElement("name","network");
	
	writer.writeStartElement("Placemark");
	writer.writeTextElement("name","Link: 709458988");
	writer.writeStartElement("Timespan");
	writer.writeTextElement("begin","2014-06-03T08:00:00Z");
	writer.writeTextElement("end","2014-06-03T08:01:00Z");
	writer.writeEndElement();//Timespan
	writer.writeTextElement("styleUrl","#style0");
	writer.writeStartElement("LineString");
	writer.writeTextElement("coordinates","18.06199,59.32729 18.06118,59.32771 18.06063,59.32801");
	writer.writeEndElement();//LineString
	writer.writeEndElement();//Placemark
	
	writer.writeEndElement();//Folder

	writer.writeEndElement(); // Document
    writer.writeEndDocument(); // kml
	file.close();
	
	return 0;
}
*/