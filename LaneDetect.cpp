#include <iostream>
#include "opencv2/opencv.hpp"
#include "CurrnetLane.h"
#include "CLine.h"
#include "LaneDetect.h"

#define MIN_AREA_PIXELS 300
#define LINE_SPACING_PIXELS 30

using namespace cv;
using namespace std;

Mat LaneDetect::preprocessing(Mat frame, Rect roi, Rect roileft, Rect roiright) {
	/*��ó��*/
	Mat	grayImage, otsuImage, closedImage, blurImage;

	cvtColor(frame, grayImage, COLOR_BGR2GRAY);	//�׷��̽����Ϸ� ��ȯ
	adaptiveThreshold(grayImage, otsuImage, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 81, -50);//������threshold
	Mat morphfillter(4, 4, CV_8U, Scalar(1)); //��������
	morphologyEx(otsuImage, closedImage, MORPH_CLOSE, morphfillter);	//Ŭ��¡����					
	medianBlur(closedImage, blurImage, 3);

	return blurImage;
}

int LaneDetect::extractLine(CLine * lines, int num_labels, Mat src_img, Mat img_labels, Mat stats, Mat centroids)
{
	int line_index = 0;

	vector<Vec4i> blob_lines;
	// Labelled image blob�� ��� üũ (i=0�� ���� ����̹Ƿ� ����)
	for (int i = 1; i < num_labels; i++)
	{
		// blob area�� �ʹ� ������ ����
		if (stats.at<int>(i, CC_STAT_AREA) < MIN_AREA_PIXELS) continue;
		// Houghtrasform �� line ���� ����,
		//line�� �ϳ��� ���� ������ ��� ���ĸ� �����ϰ� ���� blob���� �Ѿ��
		HoughLinesP(src_img, blob_lines, 1, CV_PI / 180, 100, 100, 2);

		if (blob_lines.size() == 0) continue;

		// Transform �� ����� line ���� �߿� ���� �� line�� ã�´�
		double longDistance = 0;
		int longDistanceIndex = 0;
		for (int j = 0; j < blob_lines.size(); j++)
		{
			Vec4i L = blob_lines[j];
			double distance = (L[2] - L[0])*(L[2] - L[0]) + (L[3] - L[1])*(L[3] - L[1]);
			if (distance > longDistance && distance > LINE_SPACING_PIXELS * 2) {
				longDistance = distance;
				longDistanceIndex = j;
			}
		}

		// ���� �� line ������ ���⸦ �̿��� �̹��� blob�� center�� �������� ���ο� line ����
		Vec4i L = blob_lines[longDistanceIndex];
		double rate = ((double)(L[3] - L[1])) / ((double)(L[2] - L[0]) + 0.5); // �и� 0�� �Ǵ� ���(���Ѵ� ����)�� ���� ���� �и� 0.5�� �����ش�
		int unique_line_flag = 1;

		// �̹� ������ line�� �ߺ��Ǵ�(�ʹ� �����) line�� ���� �������� �ʴ´� 
		for (int k = 0; k < line_index; k++) {
			if (lines[k].dist_to_point(Point(L[0], L[1])) < LINE_SPACING_PIXELS) unique_line_flag = 0;
		}

		// ���ο� line�� ��� �����Ͽ� lines �޸� ������ ����
		if (unique_line_flag) {
			double centerx = centroids.at<double>(i, 0);
			double centery = centroids.at<double>(i, 1);
			int left = stats.at<int>(i, CC_STAT_LEFT);
			int width = stats.at<int>(i, CC_STAT_WIDTH);
			lines[line_index] = CLine(rate, Point((int)centerx, (int)centery), left, width);
			line_index++;
		}
	}
	// ������ line ������ ��ȯ
	return line_index;
}
void LaneDetect::displayLineinfo(Mat img, CLine * lines, int num_lines, Scalar linecolor, Scalar captioncolor, int width, int height) {
	for (int i = 0; i < num_lines; i++) {
		double x1, x2, y1, y2;

		/*img�� ��ü frame�̱� ������ width, height�� ������
		roiframe�̶�� ������ �ʿ� ����*/
		x1 = lines[i].left;
		x2 = lines[i].left + lines[i].width;
		y1 = lines[i].rate * x1 + lines[i].y_inter;
		y2 = lines[i].rate * x2 + lines[i].y_inter;
		line(img, Point((int)x1 + width, (int)y1 + height), Point((int)x2 + width, (int)y2 + height), linecolor, 1, LINE_AA);


		char text[15];
		double tanval = -atan2(lines[i].rate, 1.0) / (double)CV_PI;

		if (tanval < 0)
			tanval = tanval + 1.0;
		sprintf_s(text, "%.2lfpi", tanval);
		putText(img, text, lines[i].center, 2, 0.5, captioncolor);

	}
}
void LaneDetect::detectcolor(Mat& image, double minH, double maxH, double minS, double maxS, Mat& mask) {
	Mat hsv;
	vector<Mat> channels;
	cvtColor(image, hsv, CV_BGR2HSV);
	split(hsv, channels);
	Mat mask1;
	threshold(channels[0], mask1, maxH, 255, THRESH_BINARY_INV);
	Mat mask2;
	threshold(channels[0], mask2, minH, 255, THRESH_BINARY);
	Mat hmask;
	if (minH < maxH)
		hmask = mask1 & mask2;
	else
		hmask = mask1 | mask2;

	threshold(channels[1], mask1, maxS, 255, THRESH_BINARY_INV);
	threshold(channels[1], mask2, minS, 255, THRESH_BINARY);

	Mat smask;
	smask = mask1 & mask2;
	mask = hmask & smask;

}
double LaneDetect::getAngle(Point a, Point b, Point c) {
	Point ab = { b.x - a.x,b.y - a.y };
	Point cb = { b.x - c.x,b.y - c.y };

	double inner = (ab.x*cb.x + ab.y*cb.y);
	double l1 = sqrt(ab.x*ab.x + ab.y*ab.y);
	double l2 = sqrt(cb.x*cb.x + cb.y*cb.y);

	double lx1 = ab.x / l1;
	double ly1 = ab.y / l1;
	double lx2 = cb.x / l2;
	double ly2 = cb.y / l2;

	inner = (lx1*lx2 + ly1 * ly2);
	double result = acos(inner) * 180 / CV_PI;

	return result;
}
void LaneDetect::currentLane(Mat& image, double* angle, Point &curv, Point &curx, Point &cury, CLine* lines_R, CLine* lines_L, int right_lines, int left_lines, int *check) {
	double al, bl, ar, br;
	int x, y;
	int num_lines = left_lines;
	double x1L, x2L, y1L, y2L, x1R, x2R, y1R, y2R;
	vector<double> angles;
	vector<Point> X;
	vector<Point> Y;
	vector<Point> V;
	vector<Point> ROI_Vertices;

	int index = 0;


	Mat mask = Mat(image.rows, image.cols, CV_8UC1);

	for (int k = 0; k < image.cols; k++)
		for (int m = 0; m < image.rows; m++)
			mask.at<uchar>(Point(k, m)) = 0;

	Mat imageDest = Mat(image.rows, image.cols, CV_8UC3);
	vector<Point> ROI_Poly;
	Point v;
	if (left_lines > right_lines) {
		num_lines = right_lines;

	}
	V.resize(num_lines);
	X.resize(num_lines);
	Y.resize(num_lines);
	angles.resize(num_lines);
	for (int i = 0; i < num_lines; i++) {

		x1L = lines_L[i].left;
		x2L = lines_L[i].left + lines_L[i].width;
		y1L = lines_L[i].rate * x1L + lines_L[i].y_inter;
		y2L = lines_L[i].rate * x2L + lines_L[i].y_inter;
		x1R = lines_R[i].left;
		x2R = lines_R[i].left + lines_R[i].width;
		y1R = lines_R[i].rate * x1R + lines_R[i].y_inter;
		y2R = lines_R[i].rate * x2R + lines_R[i].y_inter;

		x1R += (image.cols / 2);
		x2R += (image.cols / 2);

		al = (y2L - y1L) / (x2L - x1L);
		bl = y1L - (y2L - y1L) / (x2L - x1L)*x1L;
		ar = (y2R - y1R) / (x2R - x1R);
		br = y1R - (y2R - y1R) / (x2R - x1R)*x1R;

		x = (int)(-(bl - br) / (al - ar));
		y = (int)(al*(-(bl - br) / (al - ar)) + bl);

		if ((x >= 0 && x <= mask.cols) && (y >= 0 && y <= mask.rows))
		{

			V[i] = Point(x, y);

			X[i] = Point(lines_L[i].start.x, lines_L[i].start.y);
			Y[i] = Point(lines_R[i].end.x + image.cols / 2, lines_R[i].end.y);

			//���� ����,�����ʿ����ȿ� ������ ����
			if ((X[i].x >= 0 && X[i].x <= image.cols / 2) && (Y[i].x > image.cols / 2 && Y[i].x <= image.cols))
			{

				if (X[i].y > Y[i].y) {
					double y_inter = lines_L[i].y_inter;
					double rate = lines_L[i].rate;
					X[i].y = Y[i].y;
					X[i].x = (X[i].y - y_inter) / rate;    //y = rate * x + y_inter;y_inter = -rate*center.x + center.y;
				}
				else {
					double rate = lines_R[i].rate;
					double y_inter = lines_R[i].y_inter;
					double d = Y[i].y - X[i].y;
					Y[i].y = X[i].y;
					Y[i].x = Y[i].y *(1 / rate) + image.cols / 2;

				}
				angles[i] = getAngle(X[i], V[i], Y[i]);

				if (*angle > angles[i])
				{
					*angle = angles[i];
					index = i;

					curv = V[index];
					curx = X[index];
					cury = Y[index];

					(*check)++;
				}

				ROI_Vertices.push_back(curv);
				ROI_Vertices.push_back(curx);
				ROI_Vertices.push_back(cury);

				approxPolyDP(ROI_Vertices, ROI_Poly, 1.0, true);
				fillConvexPoly(mask, &ROI_Poly[0], ROI_Poly.size(), 255, 8, 0);


				image.copyTo(imageDest, mask);
			}

		}

	}


}