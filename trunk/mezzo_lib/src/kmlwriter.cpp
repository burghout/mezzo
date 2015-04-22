#include "kmlwriter.h"

void KmlWriter::init()
{
	file.setFileName(filename);

    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cout << "Unable to write to " << filename.toStdString();
    } 
	else
	{
		//Initialise writer with KML headers etc
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
	}
}

void KmlWriter::close()
{
	writer.writeEndElement(); //Document
	writer.writeEndElement(); //kml
	file.close();
}
void KmlWriter::add_style(QString id, QString color, QString width)
{
	writer.writeStartElement("Style");
	writer.writeAttribute ("id", id);
	writer.writeStartElement("LineStyle");
	writer.writeTextElement ("color", color);
	writer.writeTextElement ("width", width);
	writer.writeEndElement(); //LineStyle
	writer.writeEndElement(); // Style	
}

void KmlWriter::start_folder(QString name)
{
	writer.writeStartElement("Folder");
	writer.writeTextElement("name",name);
}

void KmlWriter::write_link_placemark(QString name, QString begin, QString end, QString style, QString coordinates)
{
	writer.writeStartElement("Placemark");
	writer.writeTextElement("name",name);
	writer.writeStartElement("Timespan");
	writer.writeTextElement("begin",begin);
	writer.writeTextElement("end",end);
	writer.writeEndElement();//Timespan
	writer.writeTextElement("styleUrl",style);
	writer.writeStartElement("LineString");
	writer.writeTextElement("coordinates",coordinates);
	writer.writeEndElement();//LineString
	writer.writeEndElement();//Placemark
}